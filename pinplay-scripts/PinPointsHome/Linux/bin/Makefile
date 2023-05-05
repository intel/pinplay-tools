simpoint:
	wget -O - http://cseweb.ucsd.edu/~calder/simpoint/releases/SimPoint.3.2.tar.gz | tar -x -f - -z
	patch --directory=SimPoint.3.2 --strip=1 < simpoint_modern_gcc.patch
	make -C SimPoint.3.2
	ln -s SimPoint.3.2/bin/simpoint ./simpoint

