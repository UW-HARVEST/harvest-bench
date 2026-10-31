#!/usr/bin/env python3
"""Freeze/audit the complete declared public ABI, using DARPA headers and nm.

Maintenance only: python3 abi_audit.py TEST_CASE REFERENCE_SO OUTPUT_JSON
"""
import json
from pathlib import Path
import subprocess
import sys
import tempfile


def inventory(source, library):
    headers = ['include/sqlite3.h', 'include/tclext.h',
               'src/ext/session/sqlite3session.h',
               'src/ext/recover/sqlite3recover.h',
               'src/ext/rtree/sqlite3rtree.h', 'src/ext/fts5/fts5.h',
               'src/ext/fts3/fts3_tokenizer.h', 'src/ext/qrf/qrf.h']
    declarations = {}
    for header in headers:
        with tempfile.TemporaryDirectory() as tmp:
            unit = Path(tmp) / 'api.c'
            unit.write_text('#include "' + str((source / header).resolve()) + '"\n')
            ast = json.loads(subprocess.check_output([
                'clang', '-x', 'c', '-fsyntax-only', '-Xclang', '-ast-dump=json',
                '-DSQLITE_ENABLE_SESSION', '-DSQLITE_ENABLE_PREUPDATE_HOOK',
                '-I' + str(source / 'include'), str(unit)], text=True))
        for node in ast['inner']:
            name = node.get('name', '')
            if node['kind'] in ('FunctionDecl', 'VarDecl') and name.startswith('sqlite3'):
                declarations.setdefault(name, {'type': node['type']['qualType'],
                                               'declared_by': header})
    # DARPA's static-extension loader declares two initializers locally;
    # its tclext.h uses older names. They are public extension entry points.
    for name in ('sqlite3_basexx_init', 'sqlite3_spellfix_init'):
        declarations[name] = {'type': 'int (sqlite3 *, char **, const sqlite3_api_routines *)',
                              'declared_by': 'src/tclsqlite.c (static extension loader)'}
    exported = {line.split()[-1] for line in subprocess.check_output(
        ['nm', '-D', '--defined-only', str(library)], text=True).splitlines() if line.split()}
    public = sorted(exported & declarations.keys())
    return {'public': {name: declarations[name] for name in public},
            'not_in_fixed_build': {name: declarations[name] for name in sorted(declarations.keys() - exported)},
            'internal_exports': sorted(exported - declarations.keys()),
            'test_wrapper_exports': [],
            'headers': headers}


if __name__ == '__main__':
    source, library, output = map(Path, sys.argv[1:])
    result = inventory(source, library)
    output.write_text(json.dumps(result, indent=2) + '\n')
    print(f"{len(result['public'])} public, {len(result['internal_exports'])} internal, "
          f"{len(result['not_in_fixed_build'])} declarations absent from fixed C build")
