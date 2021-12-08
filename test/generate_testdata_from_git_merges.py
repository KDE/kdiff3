#!/usr/bin/env python

# SPDX-FileCopyrightText: 2002-2007 Joachim Eibl, joachim.eibl at gmx.de
# SPDX-License-Identifier: GPL-2.0-or-later

import argparse
import glob
import os
import subprocess as sp
import sys

# Prior to this python for windows still uses legancy non uft-8 encoding by default.
assert sys.version_info >= (3, 7)

parser = argparse.ArgumentParser(formatter_class=argparse.RawDescriptionHelpFormatter,
                                 description='Generate input files for alignmenttest from the files merged for each merge commit in a git repository.\n\n' +
                                             'This script finds all merge commits in the clone where it is run, checks which files were modified in both\n' +
                                             'parents of the merge commit and then finds the common ancestor of these files to get the merge base.\n\n'
                                             'Example:\n'
                                             '  cd ~/git/linux\n'
                                             f'  ~/kdiff3/test/{os.path.basename(sys.argv[0])} -d ~/kdiff3/test/testdata/linux\n')

parser.add_argument('-d', metavar='destination_path', nargs=1, default=['testdata_from_git/'],
                    help='specify the folder where to save the test input files. If the folder does not exist it will be created.')
args = parser.parse_args()
dirname = args.d[0]

print(f'Generating input files in {dirname} ...')
sys.stdout.flush()

if not os.path.exists(dirname):
    os.makedirs(dirname)

merges = sp.check_output('git rev-list --merges --parents master'.split()).strip().decode()

for entry in merges.splitlines():
    fields = entry.split()

    if len(fields) > 3:
        print(f'merge {fields[0]} had more than 2 parents: {fields}')
    merge, contrib1, contrib2 = fields[:3]

    if glob.glob(f'{dirname}/{merge}_*'):
        print(f'skipping merge {merge} because files for this merge already present')
        continue

    base = sp.check_output((f'git merge-base {contrib1} {contrib2}').split()).strip().decode()

    fileschanged1 = sp.check_output((f'git diff --name-only {base} {contrib1}').split()).strip().decode().splitlines()
    fileschanged2 = sp.check_output((f'git diff --name-only {base} {contrib2}').split()).strip().decode().splitlines()

    fileschangedboth = set(fileschanged1) & set(fileschanged2)

    if not fileschangedboth:
        print(f'No files overlapped for merge {merge}')
    else:
        print(f'Overlapping files for merge {merge} with base {base}: {fileschangedboth}')
        for filename in fileschangedboth:
            simplified_filename = filename.replace('/', '_').replace('.', '_')

            try:
                base_content = sp.check_output((f'git show {base}:{filename}').split())
                contrib1_content = sp.check_output((f'git show {contrib1}:{filename}').split())
                contrib2_content = sp.check_output((f'git show {contrib2}:{filename}').split())

                if base_content in [contrib1_content, contrib2_content] or \
                   contrib1_content == contrib2_content:
                   print('this merge was trivial. Skipping.')
                else:
                    basefilename = f'{dirname}/{merge}_{simplified_filename}_base.txt'
                    contrib1filename = f'{dirname}/{merge}_{simplified_filename}_contrib1.txt'
                    contrib2filename = f'{dirname}/{merge}_{simplified_filename}_contrib2.txt'

                    for filename, content in [(basefilename, base_content),
                                              (contrib1filename, contrib1_content),
                                              (contrib2filename, contrib2_content)]:
                        with open(filename, 'wb') as f:
                            f.write(content)

                    with open(f'{dirname}/{merge}_{simplified_filename}_expected_result.txt', 'a') as f:
                        pass

            except sp.CalledProcessError:
                print('error from git show, continuing with next file')

print('Input files generated.')
print('')
print(f'To create a reference set of expected_result.txt files, run alignmenttest and copy/move all {dirname}/*_actual_result.txt files to {dirname}/*_expected_result.txt:')
print('  ./alignmenttest > /dev/null')
print(f'  cd {dirname}')
print('  for file in *_actual_result.txt; do mv ${file} ${file/actual/expected}; done')
print("If you've already modified the algorithm, you can run the alignment test of an older version of kdiff3 and copy those result files over")
