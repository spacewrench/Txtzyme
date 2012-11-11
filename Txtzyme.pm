#
# Txtzyme interface functions
#
package Txtzyme;

use strict;
use Exporter;
use FileHandle;
use vars qw($VERSION @ISA @EXPORT @EXPORT_OK %EXPORT_TAGS);

$VERSION = 1.00;
@ISA = qw(Exporter);
@EXPORT = ();
@EXPORT_OK = ();
%EXPORT_TAGS = ( DEFAULT => [] );

sub new {
    my $class = shift;
    my $self = {};

    bless $self, $class;

    $self->{port} = shift || '/dev/ttyACM0';
    $self->{fh} = FileHandle->new( '+<' . $self->{port} ) or die "Can't open " . $self->{port} . ": $!";

    my $tmpfh = select $self->{fh};
    $| = 1;
    select $tmpfh;

    $self->safe;

    return $self;
}

sub DESTROY {
    my $self = shift;

    if ($self->{fh}) {
        $self->safe;
        $self->{fh}->close;
    }
}

#
# Switch all pins to input with pull-ups off.
#
sub safe {
    my $self = shift;
    $self->putz( '8{kazkbzkczkdzkezkfz}' );	# Switch all pins to input, pull-ups off!
}

sub putz {
    my $self = shift;

#    print STDERR '>> ', @_, "\n";
    $self->{fh}->print( @_, "\n" );
}

sub getz {
    my $self = shift;

    $self->putz( @_ ) unless $#_ < 0;

    my $line = $self->{fh}->getline( );

    $line =~ s/\s*$//og;

#    print STDERR '<< ', $line, "\n";

    return $line;
}

1;
