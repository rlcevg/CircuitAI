#!/usr/bin/env python3
# -*- coding: utf-8 -*-

from subprocess import Popen, PIPE
import shutil


def main():
	for i in range(100):
		print ("---------- TEST RUN {:02d} ----------".format(i))
		process = Popen(["./spring-headless", "head.script"], stdout=PIPE)
		(output, err) = process.communicate()
		exit_code = process.wait()
		if exit_code != 0:
			shutil.copy2("./infolog.txt", "./infolog_{0:02d}.txt".format(i))


if __name__ == "__main__":
    main()
