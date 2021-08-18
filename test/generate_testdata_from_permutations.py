#!/usr/bin/env python

# SPDX-FileCopyrightText: 2002-2007 Joachim Eibl, joachim.eibl at gmx.de
# SPDX-License-Identifier: GPL-2.0-or-later

import argparse
import os
import random
import sys

dirname = 'testdata/permutations'

defaultlines = ['aaa\n',
                'bbb\n',
                'ccc\n',
                'ddd\n',
                'eee\n']

# For the lines of the A file only consider removing them because modifying
# them ("diff") would be equivalent to modifying both B and C, so that will
# be covered anyway.
options = [ ('1','1','1'),
            ('1','1','2'),
            ('1','1',None),
            ('1','2','1'),
            ('1','2','2'),
            ('1','2','3'),
            ('1','2',None),
            (None,'1','1'),
            (None,'1','2'),
            (None,'1',None),
            (None,None,'1') ]

def permutations(nr_of_options, count, currentlist):

    if count == 0:
        filename = ''.join([format(i, '1x') for i in currentlist])

        baselines = []
        contrib1lines = []
        contrib2lines = []
        for optionindex, defaultline in zip(currentlist, defaultlines):
            option = options[optionindex]

            if option[0]:
                baselines.append(defaultline)

            if option[1] == '1':
                contrib1lines.append(defaultline)
            elif option[1] == '2':
                contrib1lines.append('xxx' + defaultline)

            if option[2] == '1':
                contrib2lines.append(defaultline)
            elif option[2] == '2':
                contrib2lines.append('xxx' + defaultline)
            elif option[2] == '3':
                contrib2lines.append('yyy' + defaultline)

        with open(f'{dirname}/perm_{filename}_base.txt', 'wb') as f:
            f.writelines(baselines)

        with open(f'{dirname}/perm_{filename}_contrib1.txt', 'wb') as f:
            f.writelines(contrib1lines)

        with open(f'{dirname}/perm_{filename}_contrib2.txt', 'wb') as f:
            f.writelines(contrib2lines)

        with open(f'{dirname}/perm_{filename}_expected_result.txt', 'a') as f:
            pass

    else:
        optionindices = random.sample(range(len(options)), nr_of_options)
        for optionindex in optionindices:
            permutations(nr_of_options, count - 1, [optionindex] + currentlist)


parser = argparse.ArgumentParser(formatter_class=argparse.RawDescriptionHelpFormatter,
                                 description='Generate input files for alignmenttest in ./testdata/permutations/ containing some or all permutations of 3 sets of 5 lines.\n\n' +
                                             'Everything is based on a default set of 5 different lines: aaa, bbb, ccc, ddd and eee.\n' +
                                             'For the base file each line will be either equal to the default set or removed.\n' +
                                             "For contributor 1 each line will either be equal to the default set, different than the default set('xxx' prepended) or removed.\n" +
                                             "For contributor 2 each line will either be equal to the default set, equal to contributor 1, different('yyy' prepended) or removed.\n" +
                                             f"This results in {len(options) ** len(defaultlines)} possible permutations. The -r option can be used to make a smaller 'random' selection (the same seed is used each time).")

parser.add_argument('-r', metavar='num', nargs='?', type=int, default=len(options), const=len(options),
                    help=f'instead of generating all {len(options)} permutations for each line, generate <num> randomly chosen ones. The number of test cases will become num^5.')
parser.add_argument('-s', metavar='num', nargs='?', type=int, default=0, const=0,
                    help='specify the seed to use for the random number generator (default=0). This only makes sense when the -r option is specified.')
args = parser.parse_args()

if not os.path.exists(dirname):
    os.makedirs(dirname)

print(f'Generating input files in {dirname} ...')
sys.stdout.flush()

random.seed(args.s)
permutations(args.r, len(defaultlines), [])

print('Input files generated.')
print('')
print(f'To create a reference set of expected_result.txt files, run alignmenttest and copy/move all {dirname}/*_actual_result.txt files to {dirname}/*_expected_result.txt:')
print('  ./alignmenttest > /dev/null')
print(f'  cd {dirname}')
print('  for file in *_actual_result.txt; do mv ${file} ${file/actual/expected}; done')
print("If you've already modified the algorithm, you can run the alignment test of an older version of kdiff3 and copy those result files over")
