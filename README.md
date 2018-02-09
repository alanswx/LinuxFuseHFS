=== LinuxFuseHFS

This project is a slight rewrite of this great project:
https://github.com/thejoelpatrol/fusehfs

I didn't fork it, because this version is meant to run on linux, and not on macos. Instead of setting up the xtended attributes for the resource fork, it attemps to do something smart with them.  ie: if it detects that a file has a resource fork and a data fork, it will make a MacBinaryII file (virtually) out of it.  The idea is that you can copy files off of the drive, and onto another unix platform and you will end up with a useful MacBinaryII file. If you copy from one HFS to another, it should properly reconstruct the MacBinary files and give you the resource forks.

Eventually I would like to treat text files, and other special files in a smart way, ie: if we have TEXT as a filetype, we can put a .txt on the end of the filename, etc.


