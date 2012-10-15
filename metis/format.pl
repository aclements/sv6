#!/usr/bin/perl

# This file automatically format all the .c and .h files in this project
# using indent.

use File::Find;
use Shell;
my $dir = pwd();
$dir = trim($dir);
find(\&edits, $dir);
sub trim($)
{
    my $string = shift;
    $string =~ s/^\s+//;
    $string =~ s/\s+$//;
    return $string;
}

sub edits() 
{
    my $opts = "-nbad -bap -nbc -bbo -br -brs -ncdb -ce -cp1 -cs -di4 -ndj -nfc1 -nfca -hnl -i4 -lp -npcs -nprs -psl -saf -sai -saw -nsc -nsob -ss";
    if ($File::Find::name =~ /\.c$/ || $File::Find::name =~ /\.h$/) 
    {
	system("indent $opts $File::Find::name");
	system("rm $File::Find::name~");
    }
}
