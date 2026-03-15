#!/usr/bin/env python3

from collections import OrderedDict
from os import system, getcwd, chdir
from os.path import isfile
from subprocess import Popen, PIPE
from queue import Empty
from multiprocessing import Process, Queue
import os
import getpass
import re

folder_name = os.getcwd().split(os.sep)[-1]
if folder_name == 'test-scripts':
    HERE = './'
elif folder_name == 'p2shell':
    HERE = './test-scripts'
else:
    print('Please run grade script inside p3shell')
    exit(0)

PBF = '../batch-files'
PEF = '../expected-output'
SOLN = "../"
shell_path = SOLN + 'myshell'

TIMEOUT = 5

batchfiles = OrderedDict((('gooduser_basic', 7),
                          ('gooduser_args', 7),
                          ('gooduser_redirection', 7),
                          ('gooduser_multipleCommand', 7),
                          ('buildin_wrongFormat', 7),
                          ('cd_toFolderNotExist', 3),
                          ('badCommand', 4),
                          ('complexCommand', 6),
                          ('multipleCommand', 8),
                          ('complexRedirection_format', 6),
                          ('advancedRedirection_format', 6),
                          ('complexRedirection_illegal', 6),
                          ('advancedRedirection_illegal', 6),
                          ('advancedRedirection_concat', 6),
                          ('emptyInput', 1),
                          ('1KCommand', 3)))

aux_files = {'complexRedirection_format': range(1, 9),
             'complexRedirection_illegal': range(1, 2),
             'advancedRedirection_format': range(1, 9),
             'advancedRedirection_illegal': range(1, 2),
             'advancedRedirection_concat': range(1, 2),
             'gooduser_redirection': range(1, 3)}


def isProcessExist(pname):
    usrname = getpass.getuser()
    proc1 = Popen(['pgrep', '-u', usrname, pname], stdout=PIPE, stderr=PIPE)
    out, _ = proc1.communicate()
    return (out != b'')


def __safeExec(child, result_queue):
    result_queue.put(child.communicate(''))
    return


def safeExec(cli):
    q = Queue()
    child = Popen(cli, shell=True, stdout=PIPE, stderr=PIPE, stdin=PIPE)
    t = Process(target=__safeExec, args=(child, q))
    t.start()
    try:
        output = q.get(True, TIMEOUT)
    except Empty:
        child.kill()
        t.terminate()
        t.join()
        print('Timed out')
        while (isProcessExist('myshell')):
            print('myshell exist')
            system('killall -r myshell')
        return (True, (b'', b''))
    t.join()
    return (False, output)


def takeDiff(file1, file2):
    system('chmod u+r {}'.format(file1))
    exec_result = safeExec('diff {} {}'.format(file1, file2))
    stdout = exec_result[1][0].decode("utf-8")
    print(stdout)
    stderr = exec_result[1][1].decode("utf-8")
    print(stderr)
    if stdout.strip() == '' and stderr.strip() == '':
        # success
        return True
    return False


def runBatch(bf):
    print("----------------------------------")
    print("Running batch file: " + bf)

    if not isfile(shell_path):
        print(shell_path + " does not exist")
        return False

    exec_result = safeExec(
        "{shell_path} {PBF}/{bf} 1> out/{bf}.stdout 2> out/{bf}.stderr".format(shell_path=shell_path, PBF=PBF, bf=bf))
    if exec_result[0]:
        # timed out
        return False

    extensions = [('out/', '.stdout'), ('out/', '.stderr')]
    try:
        aux = aux_files[bf]
    except KeyError:
        aux = ()
    for num in aux:
        extensions.append(('', '_rd_{:d}'.format(num)))

    output_match = True
    for extension in extensions:
        filename = bf + extension[1]
        print('Diffing ' + filename)
    # print extension[0] + filename + " vs " + PEF + '/' + filename
        output_match = output_match and takeDiff(
            extension[0] + filename, PEF + '/' + filename)
    return output_match


def scorePrint(name, score, potential):
    padding = ' '*(20-len(name))
    print('{}{} {:d}  {:d}'.format(name, padding, score, potential))


def runAll():
    system('mkdir out')
    here = getcwd()
    chdir(SOLN)
    safeExec('make')
    chdir(here)
    totalScore = 0
    for bf in batchfiles:
        score = 0
        potential = batchfiles[bf]
        if runBatch(bf):
            score = potential
        scorePrint(bf, score, potential)
        totalScore += score
    # run 2 batch files
    score = 0
    potential = 2
    exec_result = safeExec(
        "{shell_path} {PBF}/gooduser {PBF}/badCommand".format(shell_path=shell_path, PBF=PBF))
    if (not exec_result[0]) and (exec_result[1][0].decode("utf-8").strip() == 'An error has occurred'):
        score = potential
    scorePrint('two arguments', score, potential)
    totalScore += score
    # run non-existant batch file
    score = 0
    potential = 2
    if isfile('notafile'):
        system('rm notafile')
    exec_result = safeExec("{shell_path} notafile".format(
        shell_path=shell_path, PBF=PBF))
    if (not exec_result[0]) and exec_result[1][0].decode("utf-8").strip() == 'An error has occurred':
        score = potential
    scorePrint('does not exist', score, potential)
    totalScore += score
    # "File name"
    score = 0
    potential = 1
    score = potential
    scorePrint('File name', score, potential)
    totalScore += score
    # README and Makefile
    score = 0
    potential = 5
    if isfile(SOLN + 'README') and isfile(SOLN + 'Makefile'):
        score = potential
    scorePrint('Makefile and README', score, potential)
    totalScore += score
    scorePrint('total', totalScore, 100)
    return totalScore

# Checks and reminds student to do incremental commits
def checkIncrementalCommit():
    incr_file = os.path.join(SOLN, 'contrib-commits.txt')
    if not isfile(incr_file):
        print("WARNING: Incremental commit file 'contrib-commits.txt' not found in project root directory.")
        return 
    # If exist, check for at least 4 lines.
    with open(incr_file, 'r') as f:
        lines = f.readlines()
        if len(lines) < 4:
            print("WARNING: Your incremental commit file 'contrib-commits.txt' has less than 4 lines")
            print("Please remember to make incremental commits to checkpoint your progress and prevent score deduction.")
            print("Format of a line should be (expected score, github commit link, description) pair:")
            print("e.g. 14, https://proj.cs.uchicago.edu/cmsc-14400-winter-2026/williamn/-/commit/COMMIT_HASH, pass test 1\n")

def checkAcademicHonesty():
    ah_file = os.path.join(SOLN, 'academic-honesty.txt')
    if not isfile(ah_file):
        print("WARNING: Academic honesty file 'academic-honesty.txt' not found in project root directory.")
        print("Please include this file with your submission to avoid score deduction.")
        return

    if isfile(ah_file):
        with open(ah_file, 'r') as f:
            content = f.read()
            if 'CNET' in content or 'cnet' in content:
                print("WARNING: Please fill in your CNET ID in the 'academic-honesty.txt' file.\n")
                return

def checkGradeDiff(totalScore):
    expected_file = os.path.join(SOLN, 'expected-grade.txt')
    if not isfile(expected_file):
        print("WARNING: Expected grade file 'expected-grade.txt' not found in project root directory.")
        print("Please include this file with your submission to avoid score deduction.")
        return

    with open(expected_file, 'r') as f:
        content = f.read()

    # Extract first integer (supports negative too)
    match = re.search(r'-?\d+', content)
    if not match:
        print("WARNING: The content of 'expected-grade.txt' does not contain a valid integer.")
        print("Please ensure that this file contains your expected grade as an integer.")
        return

    expected_score = int(match.group())

    if expected_score > totalScore:
        print(
            "Passed a new test? Congratulations! Please dont forget to commit and update your commits + expected grade file.\n"
            "Your expected grade ({}) does not match the actual grade ({})."
            .format(expected_score, totalScore)
        )
    elif expected_score < totalScore:
        print(
            "Your expected grade ({}) is less than the actual grade ({})."
            .format(expected_score, totalScore)
        )
        

#Check #expected-grade.txt and 
if __name__ == '__main__':
    thisdir = getcwd()
    chdir(HERE)
    system('./clean.sh')
    totalScore = runAll()
    print("----------------------------------")
    checkIncrementalCommit()
    checkAcademicHonesty()
    checkGradeDiff(totalScore)
    chdir(thisdir)
