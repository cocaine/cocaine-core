use 5.008;
use ExtUtils::MakeMaker;

$CC = 'g++';

$YPCFLAGS = ``;
$YPLIBS = ``;

WriteMakefile(
	NAME            => 'cocaine_dealer',
	VERSION_FROM    => 'lib/cocaine_dealer.pm',
	PREREQ_PM       => {},
	#PREFIX			=> '/usr/',
	($] >= 5.005 ?
	(ABSTRACT_FROM  => 'lib/cocaine_dealer.pm',
	AUTHOR          => 'Rim Zaidullin <creator@bash.org.ru>') : ()),
	LIBS            => ['-L../ -lcocaine-dealer ' . $YPLIBS],
	DEFINE          => '',
	INC             => '-pthread -I. -I../include/ ' . $YPCFLAGS,
	XSOPT           => '-C++',
	TYPEMAPS        => ['perlobject.map'],
	CC              => $CC,
	LD              => '$(CC)',
	OBJECT          => '$(O_FILES)'
);
