"""Portable standard-library demonstration runner. Never overwrites previous runs."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import time
import uuid


def cases():
    return [
        ('cornell', 'cornell', [], 'Diffuse, mirror, glass, area emission and indirect light'),
        ('point', 'point', [], 'Point light and hard visibility shadows'),
        ('wood-uv', 'texture', [], 'Original wood color texture on UV square'),
        ('dof-off', 'dof', ['--lens-radius', '0', '--focus-distance', '6'], 'Pinhole reference'),
        ('dof-on', 'dof', ['--lens-radius', '.22', '--focus-distance', '6'], 'Only lens radius changes'),
        ('motion-instant', 'motion', ['--shutter-open', '.5', '--shutter-close', '.5'], 'Instant at exposure midpoint'),
        ('motion-exposure', 'motion', ['--shutter-open', '0', '--shutter-close', '1'], 'Only shutter interval changes'),
        ('alpha-pattern', 'alpha-pattern', [], 'Original leaf coverage and projected shadow'),
        ('alpha-half', 'alpha-half', [], 'One 0.5 coverage layer'),
        ('alpha-double', 'alpha-double', [], 'Two 0.5 layers; geometry differs, overlapping shadow transmission 0.25'),
        ('hdr-0', 'sampling', ['--environment', 'studio-hdr.pfm', '--env-rotation', '0'], 'HDR orientation reference'),
        ('hdr-90', 'sampling', ['--environment', 'studio-hdr.pfm', '--env-rotation', '90'], 'Only HDR azimuth changes'),
        ('forest-v05', 'forest-v05', ['--environment', 'studio-hdr.pfm', '--env-rotation', '115', '--env-intensity', '1.1'], 'Current visual; mirror creek, no lens blur'),
    ]


def git_state(repository):
    def read(*arguments):
        try:
            result = subprocess.run(['git', '-C', str(repository), *arguments],
                                    capture_output=True, text=True, check=True)
            return result.stdout.strip()
        except (OSError, subprocess.CalledProcessError):
            return None
    commit = read('rev-parse', 'HEAD')
    status = read('status', '--porcelain', '--untracked-files=normal')
    return {'head': commit, 'dirty': None if status is None else bool(status),
            'status_porcelain': status,
            'provenance_note': 'HEAD identifies history only; dirty/untracked files and local executable may differ from HEAD.'}


def validate_ppm(path, width, height):
    tokens = path.read_text(encoding='ascii').split()
    if tokens[:4] != ['P3', str(width), str(height), '255']:
        raise ValueError('Unexpected PPM header')
    values = [int(token) for token in tokens[4:]]
    if len(values) != width * height * 3 or any(v < 0 or v > 255 for v in values):
        raise ValueError('Incomplete or out-of-range PPM pixels')
    return {'bytes': path.stat().st_size, 'sha256': hashlib.sha256(path.read_bytes()).hexdigest(),
            'width': width, 'height': height, 'complete': True}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', required=True, type=Path)
    parser.add_argument('--resources', required=True, type=Path, help='Assets directory')
    parser.add_argument('--output', required=True, type=Path, help='Parent for unique run directories')
    parser.add_argument('--profile', choices=['quick', 'full'], default='quick')
    parser.add_argument('--threads', type=int, default=4)
    parser.add_argument('--list', action='store_true', help='Print JSON argument lists, without running or creating files')
    args = parser.parse_args()
    if not 1 <= args.threads <= 256:
        parser.error('--threads must be in [1,256]')
    executable, resources = args.executable.resolve(), args.resources.resolve()
    width, height, spp = (160, 100, 16) if args.profile == 'quick' else (640, 400, 128)
    stamp = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ')
    directory = args.output.resolve() / ('demo-' + args.profile + '-' + stamp + '-' + uuid.uuid4().hex[:8])
    plan = []
    for name, scene, extra, description in cases():
        w, h, samples = (160, 90, 8) if name == 'forest-v05' and args.profile == 'quick' else (width, height, spp)
        if name == 'forest-v05' and args.profile == 'full':
            w, h, samples = 640, 360, 64
        output = directory / (name + '.ppm')
        command = [str(executable), '--scene', scene, '--resource-root', str(resources),
                   '--width', str(w), '--height', str(h), '--spp', str(samples),
                   '--depth', '12', '--seed', '27', '--threads', str(args.threads),
                   '--mode', 'mis', *extra, '--output', str(output)]
        plan.append({'name': name, 'scene': scene, 'description': description,
                     'command': command, 'output': str(output), 'width': w, 'height': h, 'spp': samples})
    if args.list:
        print(json.dumps(plan, indent=2, ensure_ascii=False))
        return 0
    if not executable.is_file() or not resources.is_dir():
        parser.error('Executable file and resource directory must exist')
    directory.mkdir(parents=True, exist_ok=False)
    manifest = {'profile': args.profile, 'started_utc': stamp, 'output_directory': str(directory),
                'git': git_state(Path(__file__).resolve().parents[1]),
                'executable': str(executable), 'executable_sha256': hashlib.sha256(executable.read_bytes()).hexdigest(),
                'resources': str(resources), 'runs': [], 'complete': False}
    start = time.perf_counter()
    failures = 0
    def save():
        manifest['total_wall_seconds'] = time.perf_counter() - start
        (directory / 'manifest.json').write_text(json.dumps(manifest, indent=2, ensure_ascii=False), encoding='utf-8')
    save()
    for entry in plan:
        tick = time.perf_counter()
        try:
            result = subprocess.run(entry['command'], capture_output=True, text=True)
            entry.update(exit_code=result.returncode, stdout=result.stdout, stderr=result.stderr)
            if result.returncode == 0:
                entry['image_check'] = validate_ppm(Path(entry['output']), entry['width'], entry['height'])
            else:
                entry['error'] = 'Renderer returned nonzero exit code'
        except (OSError, ValueError) as error:
            entry.setdefault('exit_code', None)
            entry['error'] = str(error)
        entry['wall_seconds'] = time.perf_counter() - tick
        if 'error' in entry:
            failures += 1
            print('FAILED scene={} exit_code={} command={} error={}'.format(
                entry['scene'], entry['exit_code'], json.dumps(entry['command']), entry['error']), file=sys.stderr)
        else:
            print('{}: {:.3f}s'.format(entry['name'], entry['wall_seconds']), flush=True)
        manifest['runs'].append(entry)
        save()
    manifest.update(complete=True, failures=failures)
    save()
    print('Output: {}\nTotal: {:.3f}s; failures: {}'.format(directory, manifest['total_wall_seconds'], failures))
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
