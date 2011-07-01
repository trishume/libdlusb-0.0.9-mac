#!/bin/sh
# $Id: init_dlusb.sh,v 1.7 2005/10/18 16:58:04 geni Exp $
#
# This script initializes the watch with applications and data (up to 14).

# read configuration file
. ~/.dlusbrc

cd $dlusb_dir

# set time
echo Setting time...
tod_dlusb tz1=$tod_tz1 tz2=$tod_tz2 tz3=$tod_tz3 date=$tod_date 24hr=$tod_24hr \
	week_no=$tod_week_no adjust=$tod_adjust

echo Recovering option...
option_dlusb < option.txt

echo Installing applications...
init=
lock=
for i in $apps
do
	case $i in
	.*)
		i=`echo $i | sed 's/^\.//'`
		lock="$lock lock=$i"
		;;
	esac
	case $i in
	*.app)
		if [ "$i" = "ModeBrow.app" ]
		then
			init="-f $init"
		fi
		init="$init wristapp=wristapp/$i"
		;;
	*=)
		init="$init $i"
		;;
	*)
		init="$init $i=$i.txt"
		;;
	esac
done

# initialize the watch and install some applications with data (up to 14)
if [ "$init" != "" ]
then
	init_dlusb $init
fi

# lock applications
if [ "$lock" != "" ]
then
	lock_dlusb $lock
fi
