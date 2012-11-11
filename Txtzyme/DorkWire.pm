#
# DorkWire functions for Txtzyme
#
package Txtzyme::DorkWire;

use strict;
use Exporter;
use FileHandle;
use vars qw($VERSION @ISA @EXPORT @EXPORT_OK %EXPORT_TAGS);

$VERSION = 1.00;
@ISA = qw(Exporter);
@EXPORT = ();
@EXPORT_OK = ();
%EXPORT_TAGS = ( DEFAULT => [] );

#
# Usage: my $ow = Txtzyme::DorkWire->new( $txtzyme, '0b' );
#
sub new {
    my $class = shift;
    my $self = { };
    
    $self->{T} = shift or die 'Txtzyme object is required';
    $self->{pin} = shift or die 'DQ pin must be specified';

    $self->{T}->putz( $self->{pin}, '1oi' );

    return bless $self, $class;
}

sub rst {
    my $self = shift;
    $self->crc;
    $self->{T}->getz( $self->{pin}, '[0o500uI68uI]p500u' );
}

sub wr {
    my $self = shift;
    $self->{T}->putz( $self->{pin}, $_[0] ? '1W' : '0W' );
}

sub w8 {
    my $self = shift;
    my ($b) = @_;
    my $cmd = $self->{pin};
    for (0..7) {
        $cmd .= (($b&1) . 'W');
        $b /= 2;
    }
    # print STDERR '>> ', $cmd, "\n";
    $self->{T}->putz( $cmd );
}

sub rd {
    my $self = shift;
    $self->{T}->getz( $self->{pin}, 'Rp' );
}

sub r8 {
    my $self = shift;
    my $byte = 0;
    $self->{T}->putz( $self->{pin}, '8{Rp}' );
    for (0..7) {
        my $bit = $self->{T}->getz( );
        $self->crc( $bit );
        $byte = ($byte >> 1) | ($bit << 7);
    }
    return $byte;
}

sub r { my $n = 0; for my $i (0..($_[0]-1)) {$b += (r8() << 8*$i)} $b }

#
# Update CRC with a bit, or return the current CRC and clear the accumulator
#
sub crc {
    my $self = shift;
    my $sr = ($self->{crc} or 0);

    if ($#_ == 0) {
        my $bit = $_[0];
        my $fb = (($sr & 1) && !$bit) || (!($sr & 1) && $bit);
        $sr >>= 1;
        $sr ^= 0x8c if ($fb);
        $self->{crc} = $sr;
    } else {
        $self->{crc} = 0;
    }
    return $sr;
}

sub crc8 {
    my $self = shift;
    my $byte = shift;

    for (0 .. 7) { $self->crc( $byte & 1 ); $byte >>= 1; }

    return $self->{crc};
}

sub enumerate {
    my $self = shift;
    my $pfx = (shift or '');
    my $targets = (shift or {});
    my $restart = sub {
        my $bits = shift;
        $self->rst;
        $self->w8( 0xF0 );
        print STDERR "SROM>\033[33;1m";
        foreach (0 .. length($bits) - 1) {
            my $b = $self->rd;
            my $nb = $self->rd;
            last if $b && $nb;
            my $bit = substr( $bits, $_, 1 );
            print STDERR $bit;
            $self->wr( $bit );
            $self->crc( $bit );
        }
        print STDERR "\033[0m";
    } ;

    if (!$pfx) {
        print STDERR "Reset & start SearchROM\n";
        &$restart( '' );
    }

    while (length( $pfx ) < 64) {
        my $b = $self->rd;
        my $notb = $self->rd;

        if ($b && !$notb || !$b && $notb) {
            print STDERR $b;
            $pfx .= $b;
            $self->crc( $b );
            $self->wr( $b );
        } elsif (!$b && !$notb) {
            print STDERR "\033[7m0\033[0m";
            $self->crc( 0 );
            $self->wr( 0 );
            $self->enumerate( $pfx . '0', $targets );
            &$restart( $pfx );
            print STDERR "\033[7m1\033[0m";
            $self->rd; $self->rd;
            $pfx .= '1';
            $self->crc( 1 );
            $self->wr( 1 );
        } else {
            print STDERR "\033[31;1m??\033[0m\n";
            &$restart( $pfx );
        }
    }

    if (!$self->crc) {
        $targets->{$pfx} = join( ':', unpack( '(H2)8', pack( 'b64', $pfx ) ) );
        printf( STDERR "\033[32;1m[OK]\033[0m\n" );
    } else {
        printf( STDERR "\033[31;1m[NG]\033[0m\n" );
    }

    return $targets;
}

# DS18B20 Thermometer Functions

sub skip {
    shift->w8( 0xCC );
}

sub cnvt {
    my $self = shift;
    $self->{T}->getz( $self->{pin}, '0WW1W0WWW1W0W1o750mzp' );	# w8 0x44
}

sub data {
    shift->w8( 0xBE );
}

#sub wdata	{ w8 0x4E; w8 $_[0]; w8 $_[1] }
#sub save	{ getz( "${pin}0WWW1W0WW1W0W1o10mip" ) }	# w8 0x48
#sub recall	{ w8 0xb8 }
#sub rrom	{ w8 0x33 }
#sub srom	{ w8 0xF0 }
#sub match	{ w8 0x55 }

# DS18B20 Thermometer Transactions (single device)

#sub all_cnvt { rst; skip; cnvt }
#sub one_data { rst; skip; data; my $c = r8; $c = r(2); $c<2**15 ? $c : $c-2**16; }

#sub temp_c { all_cnvt; 0.0625 * one_data }
#sub temp_f { 32 + 1.8 * temp_c }

1;
