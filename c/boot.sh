mkdir -pv m4
autoreconf --force --install
./configure
make
