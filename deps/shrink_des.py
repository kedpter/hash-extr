from passlib.utils.des import shrink_des_key
import sys
import re
import os

# ./shrink_des.py file_to_modify ciphertext
if __name__ == '__main__':

    hashfile = sys.argv[1]
    ciphertext = sys.argv[2]

    tmpfile = 'hash.tmp'

    with open(hashfile, 'r') as f:
        hash_lines = f.readlines()

    with open(tmpfile, 'w') as tmpf:
        for hash_line in hash_lines:
            if (ciphertext in hash_line):
                # found the one to shrink
                hash_line = hash_line.rstrip()
                hash_line_parts = hash_line.split(':')
                old_keyhash = hash_line_parts[-1]
                old_keyhash = re.sub('\$HEX\[', '', old_keyhash)
                old_keyhash = re.sub('\]', '', old_keyhash)
                new_keyhash = shrink_des_key(old_keyhash.decode('hex')).encode('hex')

                hash_line = ciphertext + ':' + new_keyhash + '\n'
            tmpf.write(hash_line)

    os.rename(tmpfile, hashfile)
