"""通过 UnrealMCP 的 execute_python：保存 /Game 下资产并打印 Pipeline Blueprint 继承信息。

用法：
- UE Editor 打开项目并加载 UnrealMCP 插件
- 运行：python UE_SAVE_AND_RECHECK.py
"""

import json
import socket

HOST = "127.0.0.1"
PORT = 55557


def send(cmd_type: str, params: dict):
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(30.0)
    sock.connect((HOST, PORT))
    sock.sendall(json.dumps({"type": cmd_type, "params": params}, ensure_ascii=False).encode("utf-8"))

    chunks = []
    while True:
        chunk = sock.recv(8192)
        if not chunk:
            break
        chunks.append(chunk)
        try:
            return json.loads(b"".join(chunks).decode("utf-8", errors="replace"))
        except Exception:
            continue
    return json.loads(b"".join(chunks).decode("utf-8", errors="replace")) if chunks else {}


PY = r"""
import unreal

print('--- Save & Recheck ---')

# 1) 强制保存目录（避免资产只存在内存/未落盘）
try:
    saved_dir = unreal.EditorAssetLibrary.save_directory('/Game', only_if_is_dirty=False, recursive=True)
    print('save_directory(/Game) =>', saved_dir)
except Exception as e:
    print('save_directory failed:', e)

# 2) 检查目标资产
bp_path = '/Game/Pipelines/BPI_FBXMaterial'
exists = unreal.EditorAssetLibrary.does_asset_exist(bp_path)
print('exists', bp_path, '=>', exists)

if exists:
    bp = unreal.load_asset(bp_path)
    print('loaded =>', bp)
    try:
        gen = bp.generated_class()
        parent = gen.get_super_struct() if gen else None
        print('generated_class =>', gen)
        print('parent =>', parent)
        if gen:
            print('is_child_of InterchangePipelineBase =>', gen.is_child_of(unreal.InterchangePipelineBase))
    except Exception as e:
        print('introspect failed:', e)

# 3) AssetRegistry 快速统计（/Game 下 Blueprint 资产数）
ar = unreal.AssetRegistryHelpers.get_asset_registry()
flt = unreal.ARFilter(class_names=['Blueprint'], package_paths=['/Game'], recursive_paths=True)
assets = ar.get_assets(flt)
print('asset_registry Blueprint assets under /Game =>', len(assets))
"""


def main():
    r = send("execute_python", {"code": PY})
    print(json.dumps(r, indent=2, ensure_ascii=False))


if __name__ == "__main__":
    main()
