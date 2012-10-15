#!/usr/bin/perl -w

use strict;

my @hdr = qw(
                 bfType
                 bfSize
                 bfReserved1
                 bfReserved2
                 bfOffBits
                 biSize
                 biWidth
                 biHeight
                 biPlanes
                 biBitCount
                 biCompression
                 biSizeImage
                 biXPelsPerMeter
                 biYPelsPerMeter
                 biClrUsed
                 biClrImportant
             );

binmode STDIN;
binmode STDOUT;

# input bmp
my $BMP = $ARGV[0];
my $OUT = $ARGV[1];


open BMP, $BMP or die $!;
binmode BMP;
my $data = <BMP>;


my @hdr_dat = unpack "SLSSLLLLSSLLLLLL", $data;
my %header;
@header{@hdr}=@hdr_dat;
print "$_\t$header{$_}\n" for @hdr;

# chop in half!
$header{bfSize} = int(($header{bfSize})*2 - 54);
$header{biSizeImage} = int(($header{biSizeImage})*2);

my $new_hdr = pack "SLSSLLLLSSLLLLLL", @header{@hdr};

open OUT, ">$OUT" or die $!;
binmode OUT;
print OUT $new_hdr;

my ($buf, $mydata, $n);
while (($n = read BMP, $mydata, 4096) != 0) {
          print OUT $mydata;
          print OUT $mydata;
}

close BMP;

close OUT;
