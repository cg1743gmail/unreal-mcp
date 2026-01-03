#include "MCPServerRunnable.h"
#include "UnrealMCPBridge.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "HAL/PlatformTime.h"

// Buffer size for receiving data
static const int32 MCP_BUFFER_SIZE = 8192;

FMCPServerRunnable::FMCPServerRunnable(UUnrealMCPBridge* InBridge, TSharedPtr<FSocket> InListenerSocket)
    : Bridge(InBridge)
    , ListenerSocket(InListenerSocket)
    , bRunning(true)
{
    UE_LOG(LogTemp, Display, TEXT("MCPServerRunnable: Created server runnable"));
}

FMCPServerRunnable::~FMCPServerRunnable()
{
    // Note: sockets are owned/destroyed by the bridge
}

bool FMCPServerRunnable::Init()
{
    return true;
}

uint32 FMCPServerRunnable::Run()
{
    UE_LOG(LogTemp, Display, TEXT("MCPServerRunnable: Server thread starting..."));

    while (bRunning)
    {
        bool bPending = false;
        if (ListenerSocket.IsValid() && ListenerSocket->HasPendingConnection(bPending) && bPending)
        {
            UE_LOG(LogTemp, Display, TEXT("MCPServerRunnable: Client connection pending, accepting..."));

            ClientSocket = MakeShareable(ListenerSocket->Accept(TEXT("MCPClient")));
            if (ClientSocket.IsValid())
            {
                UE_LOG(LogTemp, Display, TEXT("MCPServerRunnable: Client accepted"));

                // Improve stability for larger payloads
                ClientSocket->SetNoDelay(true);
                int32 SocketBufferSize = 65536;
                ClientSocket->SetSendBufferSize(SocketBufferSize, SocketBufferSize);
                ClientSocket->SetReceiveBufferSize(SocketBufferSize, SocketBufferSize);

                // Handle exactly one request per connection (current clients open a fresh TCP socket per call)
                HandleClientConnection(ClientSocket);

                ClientSocket->Close();
                ClientSocket.Reset();
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("MCPServerRunnable: Failed to accept client connection"));
            }
        }

        FPlatformProcess::Sleep(0.05f);
    }

    UE_LOG(LogTemp, Display, TEXT("MCPServerRunnable: Server thread stopping"));
    return 0;
}

void FMCPServerRunnable::Stop()
{
    bRunning = false;
}

void FMCPServerRunnable::Exit()
{
}

static bool SendAll(TSharedPtr<FSocket> Socket, const uint8* Data, int32 TotalBytes)
{
    int32 Offset = 0;
    while (Offset < TotalBytes)
    {
        int32 BytesSent = 0;
        if (!Socket->Send(Data + Offset, TotalBytes - Offset, BytesSent))
        {
            return false;
        }
        if (BytesSent <= 0)
        {
            return false;
        }
        Offset += BytesSent;
    }
    return true;
}

void FMCPServerRunnable::HandleClientConnection(TSharedPtr<FSocket> InClientSocket)
{
    if (!InClientSocket.IsValid() || !Bridge)
    {
        UE_LOG(LogTemp, Error, TEXT("MCPServerRunnable: Invalid client socket or bridge"));
        return;
    }

    // Read until we can parse a complete JSON object or timeout.
    FString MessageBuffer;
    const double StartTime = FPlatformTime::Seconds();
    const double TimeoutSeconds = 5.0;

    while (bRunning && InClientSocket.IsValid())
    {
        if ((FPlatformTime::Seconds() - StartTime) > TimeoutSeconds)
        {
            UE_LOG(LogTemp, Warning, TEXT("MCPServerRunnable: Timeout waiting for request"));
            return;
        }

        uint32 PendingSize = 0;
        if (!InClientSocket->HasPendingData(PendingSize) || PendingSize == 0)
        {
            FPlatformProcess::Sleep(0.01f);
            continue;
        }

        uint8 Buffer[MCP_BUFFER_SIZE + 1];
        int32 BytesRead = 0;
        if (!InClientSocket->Recv(Buffer, MCP_BUFFER_SIZE, BytesRead, ESocketReceiveFlags::None))
        {
            UE_LOG(LogTemp, Warning, TEXT("MCPServerRunnable: Recv failed (err=%d)"), (int32)ISocketSubsystem::Get()->GetLastErrorCode());
            return;
        }

        if (BytesRead <= 0)
        {
            UE_LOG(LogTemp, Display, TEXT("MCPServerRunnable: Client closed (zero bytes)"));
            return;
        }

        Buffer[BytesRead] = 0;
        MessageBuffer.Append(UTF8_TO_TCHAR(Buffer));

        // Try parse
        TSharedPtr<FJsonObject> JsonObject;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(MessageBuffer);
        if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
        {
            // Not complete yet, keep reading
            continue;
        }

        FString CommandType;
        if (!JsonObject->TryGetStringField(TEXT("type"), CommandType))
        {
            UE_LOG(LogTemp, Warning, TEXT("MCPServerRunnable: Missing 'type' field"));
            return;
        }

        TSharedPtr<FJsonObject> ParamsObj = MakeShareable(new FJsonObject());
        if (JsonObject->HasField(TEXT("params")))
        {
            TSharedPtr<FJsonValue> ParamsValue = JsonObject->TryGetField(TEXT("params"));
            if (ParamsValue.IsValid() && ParamsValue->Type == EJson::Object)
            {
                ParamsObj = ParamsValue->AsObject();
            }
        }

        // Propagate MCP meta (request_id / trace_id / token, etc.) into params for downstream handlers.
        if (JsonObject->HasField(TEXT("_mcp")))
        {
            const TSharedPtr<FJsonObject> McpObj = JsonObject->GetObjectField(TEXT("_mcp"));
            if (McpObj.IsValid())
            {
                ParamsObj->SetObjectField(TEXT("_mcp"), McpObj);
            }
        }

        // Execute
        FString Response = Bridge->ExecuteCommand(CommandType, ParamsObj);


        // Determine response framing
        bool bLen32Le = false;
        if (JsonObject->HasField(TEXT("_mcp")))
        {
            TSharedPtr<FJsonObject> McpObj = JsonObject->GetObjectField(TEXT("_mcp"));
            if (McpObj.IsValid())
            {
                FString Framing;
                if (McpObj->TryGetStringField(TEXT("response_framing"), Framing))
                {
                    bLen32Le = (Framing == TEXT("len32le"));
                }
            }
        }

        FTCHARToUTF8 Converter(*Response);
        const uint8* BodyBytes = (const uint8*)Converter.Get();
        const int32 BodyLen = Converter.Length();

        if (bLen32Le)
        {
            uint32 Len = (uint32)BodyLen;
            uint8 Header[4];
            Header[0] = (uint8)(Len & 0xFF);
            Header[1] = (uint8)((Len >> 8) & 0xFF);
            Header[2] = (uint8)((Len >> 16) & 0xFF);
            Header[3] = (uint8)((Len >> 24) & 0xFF);

            if (!SendAll(InClientSocket, Header, 4) || !SendAll(InClientSocket, BodyBytes, BodyLen))
            {
                UE_LOG(LogTemp, Warning, TEXT("MCPServerRunnable: Failed to send len32le response"));
            }
        }
        else
        {
            if (!SendAll(InClientSocket, BodyBytes, BodyLen))
            {
                UE_LOG(LogTemp, Warning, TEXT("MCPServerRunnable: Failed to send response"));
            }
        }

        return;
    }
}

void FMCPServerRunnable::ProcessMessage(TSharedPtr<FSocket> Client, const FString& Message)
{
    // Legacy entrypoint (currently unused). Keep behavior consistent with type/params JSON.
    if (!Client.IsValid() || !Bridge)
    {
        return;
    }

    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Message);
    if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
    {
        return;
    }

    FString CommandType;
    if (!JsonObject->TryGetStringField(TEXT("type"), CommandType))
    {
        return;
    }

    TSharedPtr<FJsonObject> ParamsObj = MakeShareable(new FJsonObject());
    if (JsonObject->HasField(TEXT("params")))
    {
        TSharedPtr<FJsonValue> ParamsValue = JsonObject->TryGetField(TEXT("params"));
        if (ParamsValue.IsValid() && ParamsValue->Type == EJson::Object)
        {
            ParamsObj = ParamsValue->AsObject();
        }
    }

    FString Response = Bridge->ExecuteCommand(CommandType, ParamsObj);
    FTCHARToUTF8 Converter(*Response);
    int32 BytesSent = 0;
    Client->Send((uint8*)Converter.Get(), Converter.Length(), BytesSent);
}
