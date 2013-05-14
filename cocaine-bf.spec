Summary:	Cocaine - Core Libraries
Name:		libcocaine-core2
Version:	0.10.3
Release:	1%{?dist}

License:	GPLv2+
Group:		System Environment/Libraries
URL:		http://www.github.com/cocaine
Source0:	%{name}-%{version}.tar.bz2
BuildRoot:	%{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

%if 0%{?rhel} < 6
BuildRequires:	gcc44 gcc44-c++
%endif
BuildRequires:	boost-python, boost-devel, boost-iostreams, boost-thread, boost-python, boost-system
BuildRequires:	zeromq-devel libev-devel
BuildRequires:  openssl-devel libtool-ltdl-devel libuuid-devel
BuildRequires:	cmake28 msgpack-devel libarchive-devel binutils-devel

Obsoletes: srw

%description
Cocaine is an open application cloud platform.

%package devel
Summary: Development files for %{name}
Group: Development/Libraries
Requires: %{name} = %{version}-%{release}

%description devel
Cocaine development headers package.

%package -n cocaine-runtime
Summary:	Cocaine - Runtime
Group:		Development/Libraries

%description -n cocaine-runtime
Cocaine runtime components package.

%package -n cocaine-tools
Summary:	Cocaine - Toolset
Group:		Development/Libraries
Requires:	%{name} = %{version}-%{release}

%description -n cocaine-tools
Various tools to query and manipulate running Cocaine instances.

%prep
%setup -q

%build
%if 0%{?rhel} < 6
export CC=gcc44
export CXX=g++44
CXXFLAGS="-pthread -I/usr/include/boost141" LDFLAGS="-L/usr/lib64/boost141" %{cmake28} -DBoost_DIR=/usr/lib64/boost141 -DBOOST_INCLUDEDIR=/usr/include/boost141 -DCMAKE_CXX_COMPILER=g++44 -DCMAKE_C_COMPILER=gcc44 .
%else
%{cmake28} .
%endif

make %{?_smp_mflags}

%install
rm -rf %{buildroot}
make install DESTDIR=%{buildroot}
rm -f %{buildroot}%{_libdir}/*.a
rm -f %{buildroot}%{_libdir}/*.la

install -dD %{buildroot}%{_sysconfdir}/init.d/
install -m 755 debian/cocaine-runtime.init %{buildroot}%{_sysconfdir}/init.d/cocaine-runtime

install -d %{buildroot}%{_sysconfdir}/cocaine
install -m644 debian/cocaine-runtime.conf %{buildroot}%{_sysconfdir}/cocaine/cocaine-default.conf

install -d %{buildroot}%{_bindir}
install -m755 scripts/cocaine-info.py %{buildroot}%{_bindir}/cocaine-info
install -m755 scripts/cocaine-start-app.py %{buildroot}%{_bindir}/cocaine-start-app
install -m755 scripts/cocaine-pause-app.py %{buildroot}%{_bindir}/cocaine-pause-app
install -m755 scripts/cocaine-check-app.py %{buildroot}%{_bindir}/cocaine-check-app
install -m755 scripts/cocaine-tool.py %{buildroot}%{_bindir}/cocaine-tool

%post -p /sbin/ldconfig
%postun -p /sbin/ldconfig

%clean
rm -rf %{buildroot}

%files
%defattr(-,root,root,-)
%doc README
%{_libdir}/*.so.*

%files devel
%defattr(-,root,root,-)
%{_includedir}/*

%files -n cocaine-runtime
%defattr(-,root,root,-)
%{_sysconfdir}/init.d/*
%{_sysconfdir}/cocaine/cocaine-default.conf
%{_bindir}/cocaine-runtime

%files -n cocaine-tools
%defattr(-,root,root,-)
%{_bindir}/cocaine-info
%{_bindir}/cocaine-start-app
%{_bindir}/cocaine-pause-app
%{_bindir}/cocaine-check-app
%{_bindir}/cocaine-tool
