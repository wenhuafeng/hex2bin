import os
import sys
import shutil

hex2bin = 'hex2bin.exe'
mot2bin = 'mot2bin.exe'
directory = './bin/Debug'

source_file = 'adc.hex'
target_file = 'adc.bin'

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

def run_exe(para):
    if para == 'y':
        os.chdir(directory)
        os.system('hex2bin.exe ' + source_file)
        cs = checksum(target_file)
        print('\r')
        print('checksum: 0x' + cs)
    else:
        print('\r')

def main(para):
    os.system('make clean')
    os.system('make')
    cp_build_file(hex2bin, directory)
    cp_build_file(mot2bin, directory)
    os.system('make clean')
    run_exe(para)

if __name__ == "__main__":
    main(sys.argv[1])