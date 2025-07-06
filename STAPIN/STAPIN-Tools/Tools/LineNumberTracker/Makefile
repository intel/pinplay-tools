tool:
	cmake --build build	
setup:
	cmake -G Ninja -B build -DPINKIT=${PIN_ROOT} -DARCH=intel64 -DSTAPIN_REPO=~/Tools/applications.instrumentation.cpu.pin.stapin
setup.debug:
	cmake -G Ninja -B build -DPINKIT=${PIN_ROOT} -DARCH=intel64 -DSTAPIN_REPO=~/Tools/applications.instrumentation.cpu.pin.stapin -DCMAKE_BUILD_TYPE=Debug
clean:
	rm -rf build
