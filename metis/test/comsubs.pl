#!/usr/bin/perl

my $sfpath = 'LD_LIBRARY_PATH=$LD_LIBRARY_PATH:obj/lib';
my $hugetlbfs = "/mnt/huge";

sub do_test {
    my $appname = shift;
    my $args = shift;
    system("rm $hugetlbfs/* >/dev/null 2>&1");
    
    my $cmd ="$sfpath ./obj/app/$appname.sf $args";
    print "$cmd\n";
    system($cmd);
    print "\n";
    #system("./obj/app/$appname $args");
};

1;
