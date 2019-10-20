#!./fsh

echo '1) i=0; while [ $i -lt 5 ] ; do i=$(($i+1)); echo $i; done'
i=0; while [ $i -lt 5 ] ; do i=$(($i+1)); echo $i; done
echo "--------------------------"

echo '2) if true ; then echo a ; else echo b; fi'
if true ; then echo a ; else echo b; fi
echo "--------------------------"

echo '3) if false ; then echo a ; else echo b; fi'
if false ; then echo a ; else echo b; fi
echo "--------------------------"

echo '4) l=p*; for f in $l; do file $f; done'
l=p*; for f in $l; do file $f; done
echo "--------------------------"

echo '5) for f in *; do case $f in *.c)echo -n c;; *.h)echo -n h;; *)echo -n " ";; esac; done && echo'
for f in *; do case $f in *.c)echo -n c;; *.h)echo -n h;; *)echo -n " ";; esac; done && echo
echo "--------------------------"

echo '6) true || echo "a" && echo "b"'
true || echo "a" && echo "b"
echo "--------------------------"
echo '7) false && echo "a" || echo "b"'
false && echo "a" || echo "b"
echo "--------------------------"

echo '8) false | echo $? | true | echo $? | false ; echo $?'
false | echo $? | true | echo $? | false ; echo $?
echo "--------------------------"

echo '9) case a in b) false;; c) false;; esac ; echo $?'
case a in b) false;; c) false;; esac ; echo $?
echo "--------------------------"

echo '10) if [ $((3**3)) -eq $((54 >>1)) ] ; then echo equal; else echo not equal; fi'
if [ $((3**3)) -eq $((54 >>1)) ] ; then echo equal; else echo not equal; fi
echo "--------------------------"

echo '11) (cd ..; pwd) ; pwd'
(cd ..; pwd) ; pwd
echo "--------------------------"

echo '12) sleep 5 & jobs'
sleep 5 & jobs
echo "--------------------------"

