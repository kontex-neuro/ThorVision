"""Extension package manifest, content digest and SDK compatibility checks.

``manifest.json`` names the SDK requirement and ``content_digest``, a SHA-256
over every other file. The package digest is the SHA-256 of the manifest
bytes, so it changes whenever any file changes; approval binds to that digest.
"""
from __future__ import annotations

import hashlib
import json
import os
import re

PACKAGE_SCHEMA = 'thorvision.package/1'
MANIFEST = 'manifest.json'
_NAME = re.compile(r'^[a-z0-9][a-z0-9_-]{0,63}$')
_HEX64 = re.compile(r'^[0-9a-f]{64}$')
_REQUIREMENT = re.compile(r'^\s*(>=|<=|==|<|>)\s*(\d+(?:\.\d+)*)\s*$')


class PackageError(ValueError):
    def __init__(self, problems):
        self.problems = list(problems)
        super().__init__('; '.join(self.problems))


def _files(directory):
    for root, folders, files in os.walk(directory):
        folders[:] = sorted(name for name in folders if name not in ('__pycache__', '.git'))
        for name in sorted(files):
            path = os.path.join(root, name)
            relative = os.path.relpath(path, directory).replace(os.sep, '/')
            if os.path.islink(path):
                raise PackageError([f'{relative}: symbolic links are not allowed'])
            if relative != MANIFEST and not name.endswith('.pyc'):
                yield relative, path


def content_digest(directory):
    digest = hashlib.sha256()
    for relative, path in _files(directory):
        with open(path, 'rb') as file:
            data = file.read()
        digest.update(f'{relative}\0{len(data)}\0'.encode() + data)
    return digest.hexdigest()


def package_digest(directory):
    with open(os.path.join(directory, MANIFEST), 'rb') as file:
        return hashlib.sha256(file.read()).hexdigest()


def seal(directory):
    """Write ``content_digest`` into the manifest and return the resulting package digest."""
    path = os.path.join(directory, MANIFEST)
    with open(path, encoding='utf-8') as file:
        manifest = json.load(file)
    manifest['content_digest'] = content_digest(directory)
    with open(path, 'w', encoding='utf-8', newline='\n') as file:
        json.dump(manifest, file, indent=2, sort_keys=True)
        file.write('\n')
    return package_digest(directory)


def _version(text):
    return tuple(int(part) for part in text.split('.'))


def satisfies(version, requirement):
    """Check a comma-separated requirement such as ``>=0.1,<0.2`` against a dotted version."""
    current = _version(version)
    for clause in requirement.split(','):
        match = _REQUIREMENT.match(clause)
        if not match:
            raise ValueError(f'unsupported requirement clause {clause!r}')
        operator, wanted = match.group(1), _version(match.group(2))
        width = max(len(current), len(wanted))
        left, right = current + (0,) * (width - len(current)), wanted + (0,) * (width - len(wanted))
        if not {'>=': left >= right, '<=': left <= right, '==': left == right,
                '<': left < right, '>': left > right}[operator]:
            return False
    return True


def load_manifest(directory, sdk_version=None):
    """Validate a package directory and return its manifest; raise ``PackageError`` otherwise."""
    from . import __version__
    sdk_version = sdk_version or __version__
    try:
        with open(os.path.join(directory, MANIFEST), encoding='utf-8') as file:
            manifest = json.load(file)
    except (OSError, ValueError) as error:
        raise PackageError([f'{MANIFEST}: {error}']) from None
    problems = []

    def need(key, kind):
        value = manifest.get(key)
        if not isinstance(value, kind):
            problems.append(f'{MANIFEST}: {key} is required')
        return value

    if manifest.get('schema') != PACKAGE_SCHEMA:
        problems.append(f'{MANIFEST}: schema must be {PACKAGE_SCHEMA}')
    name = need('name', str)
    if isinstance(name, str) and not _NAME.match(name):
        problems.append(f'{MANIFEST}: name must be lowercase letters, digits, "_" or "-"')
    need('version', str)
    entrypoint = need('entrypoint', str)
    requirement = need('sdk', str)
    if isinstance(requirement, str):
        try:
            if not satisfies(sdk_version, requirement):
                problems.append(f'{MANIFEST}: requires SDK {requirement}, device provides {sdk_version}')
        except ValueError as error:
            problems.append(f'{MANIFEST}: {error}')
    if isinstance(entrypoint, str):
        module, _, attribute = entrypoint.partition(':')
        if not module or not attribute or not os.path.isfile(os.path.join(directory, module.replace('.', '/') + '.py')):
            problems.append(f'{MANIFEST}: entrypoint {entrypoint!r} must be "module:Class" inside the package')
    for key in ('config_schema', 'dependencies'):
        value = manifest.get(key)
        if value is not None and not os.path.isfile(os.path.join(directory, value)):
            problems.append(f'{MANIFEST}: {key} file {value!r} is missing')
    for index, model in enumerate(manifest.get('models', [])):
        for key in ('name', 'path', 'sha256', 'source', 'license'):
            if not isinstance(model.get(key), str):
                problems.append(f'{MANIFEST}: models[{index}].{key} is required')
        if isinstance(model.get('sha256'), str) and not _HEX64.match(model['sha256']):
            problems.append(f'{MANIFEST}: models[{index}].sha256 must be 64 lowercase hex digits')
    for path in manifest.get('fixtures', []):
        if not os.path.exists(os.path.join(directory, path)):
            problems.append(f'{MANIFEST}: fixture {path!r} is missing')
    declared = manifest.get('content_digest')
    if declared != content_digest(directory):
        problems.append(f'{MANIFEST}: content_digest does not match the package files')
    if problems:
        raise PackageError(problems)
    return manifest
