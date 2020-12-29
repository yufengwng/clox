#!/usr/bin/env python3
#
# Adapted from https://github.com/munificent/craftinginterpreters.

from os import listdir
from os.path import abspath, dirname, isdir, join, realpath, relpath, splitext
from subprocess import Popen, PIPE

import re
import sys


REPO_DIR = dirname(realpath(__file__))
TEST_DIR = 'spec'

OUTPUT_EXPECT = re.compile(r'// expect: ?(.*)')
ERROR_EXPECT = re.compile(r'// (Error.*)')
ERROR_LINE_EXPECT = re.compile(r'// \[((java|c) )?line (\d+)\] (Error.*)')
RUNTIME_ERROR_EXPECT = re.compile(r'// expect runtime error: (.+)')
SYNTAX_ERROR_RE = re.compile(r'\[.*line (\d+)\] (Error.+)')
STACK_TRACE_RE = re.compile(r'\[line (\d+)\]')
NONTEST_RE = re.compile(r'// nontest')

EX_DATAERR = 65
EX_SOFTWARE = 70


passed = 0
failed = 0
skipped = 0
expectations = 0

interpreter = None
filter_paths = None
verbose = False


class Interpreter:
    def __init__(self):
        self.args = [join(REPO_DIR, 'clox')]
        self.language = 'c'
        self.tests = {
            TEST_DIR: 'pass',
            # These are just for earlier chapters.
            TEST_DIR + '/scanning': 'skip',
            TEST_DIR + '/expressions': 'skip',
        }

    def invoke(self, path):
        args = self.args[:]
        args.append(path)
        return Popen(args, stdin=PIPE, stdout=PIPE, stderr=PIPE)


class Test:
    def __init__(self, path):
        self.path = path
        self.output = []
        self.compile_errors = set()
        self.runtime_error_line = 0
        self.runtime_error_message = None
        self.exit_code = 0
        self.failures = []

    def read_source(self):
        with open(self.path, 'r') as source:
            for line in source:
                yield line

    def parse(self):
        global expectations
        global skipped

        # Get the path components.
        parts = self.path.split('/')
        subpath = ''
        state = None

        # Figure out the state of the test. We don't break out of this loop
        # because we want lines for more specific paths to override more
        # general ones.
        for part in parts:
            if subpath:
                subpath += '/'
            subpath += part
            if subpath in interpreter.tests:
                state = interpreter.tests[subpath]

        if not state:
            print(f"Unknown test state for '{self.path}'.")
        if state == 'skip':
            skipped += 1
            return False

        line_num = 0
        for line in self.read_source():
            line_num += 1
            match = NONTEST_RE.search(line)
            if match:
                return False

            match = OUTPUT_EXPECT.search(line)
            if match:
                self.output.append((match.group(1), line_num))
                expectations += 1
                continue

            match = ERROR_EXPECT.search(line)
            if match:
                msg = match.group(1)
                self.compile_errors.add(f"[{line_num}] {msg}")
                self.exit_code = EX_DATAERR
                expectations += 1
                continue

            match = ERROR_LINE_EXPECT.search(line)
            if match:
                lang, line, msg = match.group(2, 3, 4)
                if not lang or lang == interpreter.language:
                    self.compile_errors.add(f"[{line}] {msg}")
                    self.exit_code = EX_DATAERR
                    expectations += 1
                continue

            match = RUNTIME_ERROR_EXPECT.search(line)
            if match:
                self.runtime_error_line = line_num
                self.runtime_error_message = match.group(1)
                self.exit_code = EX_SOFTWARE
                expectations += 1

        if self.compile_errors and self.runtime_error_message:
            print(f"{pink('TEST ERROR')} {path}")
            print("    Cannot expect both compile and runtime errors.")
            print("")
            return False

        return True

    def run(self):
        global interpreter
        proc = interpreter.invoke(self.path)
        out, err = proc.communicate()
        self.validate(proc.returncode, out, err)

    def validate(self, exit_code, out, err):
        try:
            out = out.decode('utf-8').replace('\r\n', '\n')
            err = err.decode('utf-8').replace('\r\n', '\n')
        except:
            self.fail("Error decoding output.")

        # Remove the trailing last empty lines.
        error_lines = err.split('\n')
        if error_lines[-1] == '':
            del error_lines[-1]
        out_lines = out.split('\n')
        if out_lines[-1] == '':
            del out_lines[-1]

        # Validate that an expected runtime error occurred.
        if self.runtime_error_message:
            self.validate_runtime_error(error_lines)
        else:
            self.validate_compile_errors(error_lines)

        self.validate_exit_code(exit_code, error_lines)
        self.validate_output(out_lines)

    def validate_runtime_error(self, error_lines):
        if len(error_lines) < 2:
            self.fail(f"Expected runtime error '{self.runtime_error_message}' but got none.")
            return

        line = error_lines[0]
        if line != self.runtime_error_message:
            self.fail(f"Expected runtime error '{self.runtime_error_message}' but got:")
            self.fail(line)

        stack_lines = error_lines[1:]
        for line in stack_lines:
            match = STACK_TRACE_RE.search(line)
            if match:
                break

        if not match:
            self.fail(f"Expected stack trace but got: {stack_lines}")
        else:
            stack_line = int(match.group(1))
            if stack_line != self.runtime_error_line:
                self.fail(f"Expected runtime error on line {self.runtime_error_line} but was on line {stack_line}")

    def validate_compile_errors(self, error_lines):
        # Validate that every lexing/parsing error was expected.
        found_errors = set()
        num_unexpected = 0
        for line in error_lines:
            match = SYNTAX_ERROR_RE.search(line)
            if match:
                error = f"[{match.group(1)}] {match.group(2)}"
                if error in self.compile_errors:
                    found_errors.add(error)
                else:
                    if num_unexpected < 10:
                        self.fail("Unexpected error:")
                        self.fail(line)
                    num_unexpected += 1
            elif line != '':
                if num_unexpected < 10:
                    self.fail("Unexpected output on stderr:")
                    self.fail(line)
                num_unexpected += 1

        if num_unexpected > 10:
            self.fail(f"(truncated {num_unexpected - 10} more...)")

        # Validate that every expected error occurred.
        for error in self.compile_errors - found_errors:
            self.fail(f"Missing expected error: {error}")

    def validate_exit_code(self, exit_code, error_lines):
        if exit_code == self.exit_code: return

        if self.failures and self.failures[-1] != '---':
            self.failures.append('---')

        if len(error_lines) > 10:
            error_lines = error_lines[0:10]
            error_lines.append("(truncated...)")

        self.fail(f"Expected return code {self.exit_code} but got {exit_code}. Stderr:")
        self.failures += error_lines

    def validate_output(self, out_lines):
        appended = False
        def append_delimiter():
            nonlocal appended
            if not appended and self.failures and self.failures[-1] != '---':
                self.failures.append('---')
            appended = True

        idx = 0
        for line in out_lines:
            if idx >= len(self.output):
                append_delimiter()
                self.fail(f"Got output '{line}' when none was expected.")
            elif self.output[idx][0] != line:
                append_delimiter()
                msg, num = self.output[idx]
                self.fail(f"Expected output '{msg}' on line {num} but got '{line}'.")
            idx += 1

        while idx < len(self.output):
            append_delimiter()
            msg, num = self.output[idx]
            self.fail(f"Missing expected output '{msg}' on line {num}.")
            idx += 1

    def fail(self, message, *args):
        if args:
            message = message.format(*args)
        self.failures.append(message)


def status(line = None):
    # Erase the status line.
    print('\033[2K', end='')
    # Move the cursor to the beginning.
    print('\r', end='')
    if line:
        print(line, end='')
    sys.stdout.flush()


def color_text(text, color):
    # No ANSI escapes on Windows.
    if sys.platform == 'win32':
        return str(text)
    return color + str(text) + '\033[0m'


def red(text):      return color_text(text, '\033[31m')
def green(text):    return color_text(text, '\033[32m')
def yellow(text):   return color_text(text, '\033[33m')
def pink(text):     return color_text(text, '\033[91m')
def teal(text):     return color_text(text, '\033[1;36m')


def run_script(path):
    global passed
    global failed
    global skipped

    if (splitext(path)[1] != '.lox'):
        return
    if 'benchmark' in path:
        return

    # Normalize path to relative path.
    path = relpath(path).replace('\\', '/')

    # Check if we are just running a subset of the tests.
    if filter_paths:
        this_test = relpath(path, join(REPO_DIR, TEST_DIR))
        matched_filter = False
        for filter_path in filter_paths:
            if this_test.startswith(filter_path):
                matched_filter = True
                break
        if not matched_filter:
            return

    # Update the status line.
    def draw_status():
        status(f"Passed: {green(passed)} Failed: {red(failed)} Skipped: {yellow(skipped)} ({teal(path)})")
    draw_status()

    # Read the test and parse out the expectations.
    test = Test(path)
    if not test.parse():
        if verbose:
            status(f"{yellow('SKIP')}: {path}")
            print("")
            draw_status()
        return  # Either skip or non-test file.
    test.run()

    # Display the results.
    if len(test.failures) == 0:
        passed += 1
        if verbose:
            status(f"{green('PASS')}: {path}")
            print("")
            draw_status()
    else:
        failed += 1
        status(f"{red('FAIL')}: {path}")
        print("")
        for failure in test.failures:
            print(f"      {pink(failure)}")
        print("")


def walk(folder, callback):
    folder = abspath(folder)
    for entry in listdir(folder):
        nfile = join(folder, entry)
        if isdir(nfile):
            walk(nfile, callback)
        else:
            callback(nfile)


def run_suite():
    global interpreter
    global expectations
    global passed
    global failed
    global skipped

    interpreter = Interpreter()
    expectations = 0
    passed = 0
    failed = 0
    skipped = 0

    walk(join(REPO_DIR, TEST_DIR), run_script)
    status()

    successful = (failed == 0)
    if successful:
        print(f"All {green(passed)} tests passed ({expectations} expectations).")
    else:
        print(f"{green(passed)} tests passed. {red(failed)} tests failed.")

    return successful


def print_usage():
    print("Usage: test.py [-v] [filter]")


def main(args):
    global filter_paths
    global verbose

    if len(args) > 2:
        print_usage()
        sys.exit(1)
    elif len(args) == 2:
        if args[0] != '-v':
            print_usage()
            sys.exit(1)
        verbose = True
        filter_paths = args[1].split(',')
    elif len(args) == 1:
        if args[0] == '-v':
            verbose = True
        else:
            filter_paths = args[0].split(',')

    if not run_suite():
        sys.exit(1)


if __name__ == '__main__':
    main(sys.argv[1:])
