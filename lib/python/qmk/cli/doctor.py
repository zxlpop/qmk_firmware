"""QMK Doctor

Check out the user's QMK environment and make sure it's ready to compile.
"""
import platform
import shutil
import subprocess

from milc import cli
from qmk import submodules
from qmk.questions import yesno

ESSENTIAL_BINARIES = ['dfu-programmer', 'avrdude', 'dfu-util', 'avr-gcc', 'arm-none-eabi-gcc', 'bin/qmk']
ESSENTIAL_SUBMODULES = ['lib/chibios', 'lib/lufa']


def check_binaries():
    """Iterates through ESSENTIAL_BINARIES and tests them.
    """
    bin_ok = True

    for binary in ESSENTIAL_BINARIES:
        if not is_executable(binary):
            bin_ok = False

    return bin_ok


def check_submodules():
    """Iterates through all submodules to make sure they're cloned and up to date.
    """
    sub_ok = True

    for submodule in submodules.status().values():
        if submodule['status'] is None:
            if submodule['name'] in ESSENTIAL_SUBMODULES:
                cli.log.error('Submodule %s has not yet been cloned!', submodule['name'])
                sub_ok = False
            else:
                cli.log.warn('Submodule %s is not available.', submodule['name'])
        elif not submodule['status']:
            if submodule['name'] in ESSENTIAL_SUBMODULES:
                cli.log.warn('Submodule %s is not up to date!')

    return sub_ok


def is_executable(command):
    """Returns True if command exists and can be executed.
    """
    # Make sure the command is in the path.
    res = shutil.which(command)
    if res is None:
        cli.log.error("{fg_red}Can't find %s in your path.", command)
        return False

    # Make sure the command can be executed
    check = subprocess.run([command, '--version'], stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=5)
    if check.returncode in [0, 1]:  # Older versions of dfu-programmer exit 1
        cli.log.debug('Found {fg_cyan}%s', command)
        return True

    cli.log.error("{fg_red}Can't run `%s --version`", command)
    return False


def os_test_linux():
    """Run the Linux specific tests.
    """
    cli.log.info("Detected {fg_cyan}Linux.")

    if not shutil.which('systemctl'):
        cli.log.warn("Can't find systemctl to check for ModemManager.")
        return True

    mm_check = subprocess.run(['systemctl', 'list-unit-files'], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=10, universal_newlines=True)
    if mm_check.returncode == 0:
        for line in mm_check.stdout.split('\n'):
            if 'ModemManager' in line and 'enabled' in line:
                cli.log.warn("{bg_yellow}Detected ModemManager. Please disable it if you are using a Pro-Micro.")
                return False

        return True

    cli.log.error('{bg_red}Could not run `systemctl list-unit-files`:')
    cli.log.error(mm_check.stdout)
    return False


def os_test_macos():
    """Run the Mac specific tests.
    """
    cli.log.info("Detected {fg_cyan}macOS.")

    return True


def os_test_windows():
    """Run the Windows specific tests.
    """
    cli.log.info("Detected {fg_cyan}Windows.")

    return True


@cli.argument('-y', '--yes', action='store_true', arg_only=True, help='Answer yes to all questions.')
@cli.argument('-n', '--no', action='store_true', arg_only=True, help='Answer no to all questions.')
@cli.subcommand('Basic QMK environment checks')
def doctor(cli):
    """Basic QMK environment checks.

    This is currently very simple, it just checks that all the expected binaries are on your system.

    TODO(unclaimed):
        * [ ] Compile a trivial program with each compiler
        * [ ] Check for udev entries on linux
    """
    cli.log.info('QMK Doctor is checking your environment.')
    ok = True

    # Determine our OS and run platform specific tests
    OS = platform.system()

    if OS == 'Darwin':
        if not os_test_macos():
            ok = False
    elif OS == 'Linux':
        if not os_test_linux():
            ok = False
    elif OS == 'Windows':
        if not os_test_windows():
            ok = False
    else:
        cli.log.error('Unsupported OS detected: %s', OS)
        ok = False

    # Make sure the basic CLI tools we need are available and can be executed.
    bin_ok = check_binaries()

    if not bin_ok:
        if yesno('Would you like to install dependencies?', default=True):
            subprocess.run(['util/qmk_install.sh'])
            bin_ok = check_binaries()

    if bin_ok:
        cli.log.info('All dependencies are installed.')
    else:
        ok = False

    # Check out the QMK submodules
    sub_ok = check_submodules()

    if sub_ok:
        cli.log.info('Submodules are up to date.')
    else:
        if yesno('Would you like to clone the submodules?', default=True):
            submodules.update()
            sub_ok = check_submodules()

        if not sub_ok:
            ok = False

    # Report a summary of our findings to the user
    if ok:
        cli.log.info('{fg_green}QMK is ready to go')
    else:
        cli.log.info('{fg_yellow}Problems detected, please fix these problems before proceeding.')
        # FIXME(skullydazed/unclaimed): Link to a document about troubleshooting, or discord or something
