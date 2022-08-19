# gazosan

Gazo-san was rewritten in modern c++ (?) while making it faster. (This is my first time to write proper code in c++, so it may not be modern.
This is my first time to write proper code in c++, so it may not be modern.

It is at least 5 times faster as far as I can tell at hand.

Gazo-san
```bash
~/w/r/g/l/gazo-san $ time ./bin/gazosan tests/images/test_image_new.png tests/images/test_image_old.png
Start detection
Process time : 125836 - 125839

________________________________________________________
Executed in    3.21 secs    fish           external
   usr time    3.37 secs    0.08 millis    3.37 secs
   sys time    0.28 secs    1.36 millis    0.28 secs
```

gazosan
```bash
~/w/gazo-san $ time ./build/gazosan -new tests/images/test_image_new.png -old tests/images/test_image_old.png -threshold 200

________________________________________________________
Executed in  451.30 millis    fish           external
   usr time    1.26 secs      0.05 millis    1.26 secs
   sys time    0.16 secs      1.43 millis    0.16 secs
```

# Build

```bash
$ mkdir build; cd build
$ cmake -DCMAKE_BUILD_TYPE=Release ..
$ cmake --build .
$ cd ..
$ ls build
~/w/gazo-san $ ls build/
CMakeCache.txt      Makefile            gazosan
CMakeFiles          cmake_install.cmake
```

| Input | Input |
| :--: | :--: |
| test_image_old.png | test_image_new.png |
| <img src="tests/images/test_image_old.png" width="50%" /> | <img src="tests/images/test_image_new.png" width="50%" /> |

| Output |
| :--: |
| output_diff.png |
| <img src="tests/images/output_diff.png" width="80%" /> | |


There is no test yet.

# License

[Apache 2.0 license](LICENSE)

# Contributors

(In alphabetical order)
* Akari Ikenoue
* [Jye Ruey](https://github.com/rueyaa332266)
* [Naoto Kishino](https://github.com/naotospace)
* [Rikiya Hikimochi](https://github.com/hikimochi)
* [Taisuke Miyazaki](https://github.com/imishinist)
