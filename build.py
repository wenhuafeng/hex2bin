import os
import sys
import shutil

makefile_hex2bin = 'src/hex2bin.exe'
makefile_mot2bin = 'src/mot2bin.exe'
code_directory = 'src'
out_directory = 'bin/Debug'

command = 'hex2bin.exe adc.hex'
#command = 'hex2bin.exe -k 4 -f 0x08000010 -C 0x04C11DB7 0xFFFFFFFF TRUE TRUE 0xFFFFFFFF adc.hex'
target_file = 'adc.bin'

"""
makefile build support define
"""
def build():
    os.system('make clean')
    os.system('make')

def build_clean():
    os.system('make clean')

def cp_build_file(source, target):
    assert not os.path.isabs(source)

    try:
        shutil.copy(source, target)
    except IOError as e:
        print("Unable to copy file. %s" % e)
    except:
        print("Unexpected error:", sys.exc_info())

def checksum(file):
    checksum_value = 0
    with open(file, "rb") as file_pointer:
        while True:
            byte = file_pointer.read(1)
            if not byte:
                break
            checksum_value += ord(byte)
    return hex(checksum_value)[2:]

def run_exe():
    os.chdir(out_directory)
    os.system(command)
    cs = checksum(target_file)
    print('\r')
    print('checksum: 0x' + cs)
    print(os.getcwd()) # 获得当前工作目录

def makefile_build():
    # 进入src目录
    os.chdir(code_directory)
    build()

    # 切换到上级目录
    current_dir = os.path.abspath(__file__)
    os.chdir(os.path.join(current_dir, os.pardir))

    # 复制exe文件
    cp_build_file(makefile_hex2bin, out_directory)
    cp_build_file(makefile_mot2bin, out_directory)

    # 进入src目录
    os.chdir(code_directory)
    build_clean()

    # 切换到上级目录
    current_dir = os.path.abspath(__file__)
    os.chdir(os.path.join(current_dir, os.pardir))
    run_exe()

"""
cmake build support define
"""
cmake_hex2bin = 'build/hex2bin.exe'
cmake_mot2bin = 'build/mot2bin.exe'

MKDIR_BUILD_DIR = 'mkdir build'
BUILD_DIR       = './build'
CMAKE_COMMAND   = 'cmake -G"MinGW Makefiles" ../'
MAKE_CLEAN      = 'make clean'
MAKE            = 'make -j8'

def cbuild():
    os.system(MKDIR_BUILD_DIR)
    os.chdir(BUILD_DIR)
    os.system(CMAKE_COMMAND)
    os.system(MAKE_CLEAN)
    os.system(MAKE)

def cmake_build():
    cbuild()

    # 切换到上级目录
    current_dir = os.path.abspath(__file__)
    os.chdir(os.path.join(current_dir, os.pardir))

    # 复制exe文件
    cp_build_file(cmake_hex2bin, out_directory)
    cp_build_file(cmake_mot2bin, out_directory)

    run_exe()

def main(para):
    if para == 'makefile':
        makefile_build()
    elif para == 'cmake':
        cmake_build()
    else:
        print('input para error!\n')

if __name__ == "__main__":
    main(sys.argv[1])