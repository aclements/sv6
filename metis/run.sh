for i in 1 10 20 30 40 50 60 70 80
do
	echo ./obj/app/wrmem -s 4000 -p $i
	perflock ./obj/app/wrmem -s 4000 -p $i
done
