@echo off
cd source
gengetopt --include-getopt -F args < ..\args.ggo
cd..
