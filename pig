# run piglet in the GNU debugger to catch the wiley coyote

stty sane

echo "run"    > pig.tmp.$$
echo "bt"    >> pig.tmp.$$
echo "echo \\n" >> pig.tmp.$$
echo "echo \\n" >> pig.tmp.$$

echo "echo \\n" >> pig.tmp.$$
echo "echo \\n" >> pig.tmp.$$
echo "quit"  >> pig.tmp.$$
echo "yes"   >> pig.tmp.$$
echo "quit"  >> pig.tmp.$$
echo "yes"   >> pig.tmp.$$

gdb --batch  -q -command pig.tmp.$$ pig.bin | tee pigtrace

cat pigtrace  | col -b > pig.trace.$$
mv pig.trace.$$ pigtrace

# check for abnormal exit, ask user for bug report

if ! grep "exited normally" pigtrace > /dev/null
then
    echo "Piglet has crashed.  Please send a description of what you were doing"
    echo "plus the last 100 lines of the file named \"pigtrace\" to"
    echo "<walker@omnisterra.com>.  Thanks! - Rick Walker"
fi


rm pig.tmp.*
