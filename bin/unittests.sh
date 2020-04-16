#!/sh

echo Running getpid...
getpid
echo OK
echo

echo Running anon...
anon || (echo FAILED && halt 1)
echo OK
echo

echo Running schedbench...
schedbench || (echo FAILED && halt 1)
echo OK
echo

echo Running lebench...
lebench || (echo FAILED && halt 1)
echo OK
echo

echo Running usertests...
usertests || (echo FAILED && halt 1)
echo OK
echo

halt 0
