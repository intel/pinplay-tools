tool:
	cmake --build build --target install
setup:
	cmake -G Ninja -B build -DPINKIT=${PIN_ROOT} -DARCH=intel64 
setup.debug:
	cmake -G Ninja -B build -DPINKIT=${PIN_ROOT} -DARCH=intel64 -DCMAKE_BUILD_TYPE=Debug
setup.reldebug:
	cmake -G Ninja -B build -DPINKIT=${PIN_ROOT} -DARCH=intel64 -DCMAKE_BUILD_TYPE=RelWithDebInfo
clean:
	rm -rf build

