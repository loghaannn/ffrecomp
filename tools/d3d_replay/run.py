"""Run the standalone B53A6 replay: python3 tools/d3d_replay/run.py CAPTURE."""
import argparse
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
tools = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(tools))
from take_d3d import inspect_inline_capture

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('capture', type=Path)
parser.add_argument('--fifo-start', type=lambda v: int(v, 0), default=0x84001000)
parser.add_argument('--validate', action='store_true', help='require Khronos Vulkan validation layers')
args = parser.parse_args()
c = inspect_inline_capture(args.capture, args.fifo_start, replay=True)
if c['vertex_handle'] != 0x44 or c['pixel_handle'] or any(c['textures']):
    raise ValueError('expected untextured FVF 0x44 strip')
if c['program_sha256'] != '68356e69680d014d080fc2ec5c28cf033e437fe6f45349b6c9fa201980ee2958':
    raise ValueError('different transform program')
words = [c['vertex_handle'], c['topology'], c['stride'], *c['payload'], *c['surfaces']]
for aspect in ('color', 'depth'):
    words.extend(c[aspect])
words.extend([len(c['program']), *c['program']])
for field in ('constant_words', 'method_words'):
    words.append(len(c[field]))
    for k, v in c[field].items():
        words.extend([int(k, 16), int(v, 16)])

root = tools.parent
sdk = root / 'xboxrecomp'
d3d = root / 'src/d3d'
incs = [d3d, d3d/'compat', sdk/'third_party/volk', sdk/'third_party/Vulkan-Headers/include',
        sdk/'third_party/VulkanMemoryAllocator/include', sdk/'src/nv2a_vk/host']
flags = ['-I'+str(p) for p in incs] + ['-DVMA_STATIC_VULKAN_FUNCTIONS=0', '-DVMA_DYNAMIC_VULKAN_FUNCTIONS=0']
with tempfile.TemporaryDirectory(prefix='ffr-vk-replay-') as tmp:
    def run(cmd, **kw):
        subprocess.run(cmd, cwd=tmp, check=True, **kw)
    run(['clang', *flags, '-c', sdk/'third_party/volk/volk.c', '-o', 'volk.o'])
    libs = shlex.split(subprocess.check_output(['pkg-config', '--libs', 'spirv', 'glslang'], text=True))
    run(['clang++', '-std=c++20', '-Wno-nullability-completeness', *flags, tools/'d3d_replay/replay.cpp',
         *sorted((d3d/'gpu').glob('*.cpp')), d3d/'hle.cpp', d3d/'vk/spirv.cpp',
         sdk/'src/nv2a_vk/shim/util/vma.cc', 'volk.o', '-Wl,--start-group', '-lSPIRV',
         '-lglslang-default-resource-limits', *libs, '-Wl,--end-group', '-ldl', '-o', 'replay'])
    env = {k: v for k, v in os.environ.items() if not k.startswith(('FFR_', 'NV2A_'))}
    if args.validate:
        env['VK_INSTANCE_LAYERS'] = 'VK_LAYER_KHRONOS_validation'
        env['FFR_REPLAY_VALIDATE'] = '1'
    for initial_depth in ('1', '0'):
        result = subprocess.run(['./replay', (args.capture/'pre_low.bin').resolve(), initial_depth],
                                cwd=tmp, input=' '.join(map(str, words)), text=True,
                                env=dict(env, FFR_D3D_NO_CACHE='1'), stderr=subprocess.STDOUT,
                                stdout=subprocess.PIPE)
        print(result.stdout, end='')
        result.check_returncode()
        if 'Validation Error' in result.stdout or 'VUID-' in result.stdout:
            raise RuntimeError('Vulkan validation reported an error')
    for module in Path(tmp).glob('*.spv'):
        run(['spirv-val', '--target-env', 'vulkan1.1', module])
