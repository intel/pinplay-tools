tool:
	cmake --build build	
setup:
	cmake -G Ninja -B build -DPINKIT=${PIN_ROOT} -DARCH=intel64 -DSTAPIN_REPO=~/Tools/pinplay-tools/STAPIN/STAPINLibPlugin
setup.reldebug:
	cmake -G Ninja -B build -DPINKIT=${PIN_ROOT} -DARCH=intel64 -DSTAPIN_REPO=~/Tools/pinplay-tools/STAPIN/STAPINLibPlugin -DCMAKE_BUILD_TYPE=RelWithDebInfo
setup.debug:
	cmake -G Ninja -B build -DPINKIT=${PIN_ROOT} -DARCH=intel64 -DSTAPIN_REPO=~/Tools/pinplay-tools/STAPIN/STAPINLibPlugin -DCMAKE_BUILD_TYPE=Debug
clean:
	rm -rf build
