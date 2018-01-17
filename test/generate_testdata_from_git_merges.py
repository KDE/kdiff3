#!/usr/bin/env python

import argparse
import glob
import os
import subprocess as sp
import sys

parser = argparse.ArgumentParser(formatter_class=argparse.RawDescriptionHelpFormatter,
                                 description='Generate input files for alignmenttest from the files merged for each merge commit in a git repository.\n\n' +
                                             'This script finds all merge commits in the clone where it is run, checks which files were modified in both\n' +
                                             'parents of the merge commit and then finds the common ancestor of these files to get the merge base.\n\n'
                                             'Example:\n'
                                             '  cd ~/git/linux\n'
                                             '  ~/kdiff3/test/%s -d ~/kdiff3/test/testdata/linux\n' % os.path.basename(sys.argv[0]))

parser.add_argument('-d', metavar='destination_path', nargs=1, default=['testdata_from_git/'],
                    help='specify the directory where to save the test input files. If the directory does not exist it will be created.')
args = parser.parse_args()
dirname=args.d[0]

print 'Generating input files in %s ...' % dirname
sys.stdout.flush()

if not os.path.exists(dirname):
    os.makedirs(dirname)

merges = sp.check_output('git rev-list --merges --parents master'.split()).strip()

for entry in merges.splitlines():
    fields = entry.split()

    if len(fields) > 3:
        print 'merge %s had more than 2 parents: %s' % (fields[0], fields)

    merge, contrib1, contrib2 = fields[:3]

    if glob.glob('%s/%s_*' % (dirname, merge)):
        print 'skipping merge %s because files for this merge already present' % merge
        continue

    base = sp.check_output(('git merge-base %s %s' % (contrib1, contrib2)).split()).strip()

    fileschanged1 = sp.check_output(('git diff --name-only %s %s' % (base, contrib1)).split()).strip().splitlines()
    fileschanged2 = sp.check_output(('git diff --name-only %s %s' % (base, contrib2)).split()).strip().splitlines()

    fileschangedboth = set(fileschanged1) & set(fileschanged2)

    if not fileschangedboth:
        print 'No files overlapped for merge %s' % merge
    else:
        print 'Overlapping files for merge %s with base %s: %s' % (merge, base, fileschangedboth)
        for filename in fileschangedboth:
            simplified_filename = filename.replace('/', '_').replace('.', '_')

            try:
                base_content = sp.check_output(('git show %s:%s' % (base, filename)).split())
                contrib1_content = sp.check_output(('git show %s:%s' % (contrib1, filename)).split())
                contrib2_content = sp.check_output(('git show %s:%s' % (contrib2, filename)).split())

                if base_content == contrib1_content or \
                   base_content == contrib2_content or \
                   contrib1_content == contrib2_content:
                   print 'this merge was trivial. Skipping.'
                else:
                    basefilename = '%s/%s_%s_base.txt' % (dirname, merge, simplified_filename)
                    contrib1filename = '%s/%s_%s_contrib1.txt' % (dirname, merge, simplified_filename)
                    contrib2filename = '%s/%s_%s_contrib2.txt' % (dirname, merge, simplified_filename)

                    for filename, content in [(basefilename, base_content),
                                              (contrib1filename, contrib1_content),
                                              (contrib2filename, contrib2_content)]:
                        with open(filename, 'wb') as f:
                            f.write(content)

                    with open('%s/%s_%s_expected_result.txt' % (dirname, merge, simplified_filename), 'a') as f:
                        pass

            except sp.CalledProcessError:
                print 'error from git show, continuing with next file'

print 'Input files generated.'
print ''
print 'To create a reference set of expected_result.txt files, run alignmenttest and copy/move all %s/*_actual_result.txt files to %s/*_expected_result.txt:' % (dirname, dirname)
print '  ./alignmenttest > /dev/null'
print '  cd %s' % dirname
print '  for file in *_actual_result.txt; do mv ${file} ${file/actual/expected}; done'
print 'If you\'ve already modified the algorithm, you can run the alignment test of an older version of kdiff3 and copy those result files over'

