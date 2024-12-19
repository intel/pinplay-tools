#`which bash`
# Try to start or end run with a 'clean slate':
sync
sudo sh -c "sync; echo 1 > /proc/sys/vm/drop_caches"
sudo sh -c "sync; echo 2 > /proc/sys/vm/drop_caches"
sudo sh -c "sync; echo 3 > /proc/sys/vm/drop_caches"
sleep 1
sync
