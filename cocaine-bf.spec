Summary:	Cocaine - Core Libraries
Name:		libcocaine-core2
Version:	0.11.2.0
Release:	1%{?dist}

License:	GPLv2+
Group:		System Environment/Libraries
URL:		http://www.github.com/cocaine
Source0:	%{name}-%{version}.tar.bz2
BuildRoot:	%{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

%if 0%{?rhel} < 6
BuildRequires: gcc44 gcc44-c++
%endif
BuildRequires: boost-python, boost-devel, boost-iostreams, boost-thread, boost-python, boost-system
BuildRequires: libev-devel, openssl-devel, libtool-ltdl-devel, libuuid-devel, libcgroup-devel
BuildRequires: cmake28, msgpack-devel, libarchive-devel, binutils-devel

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

%prep
%setup -q

%build
%if 0%{?rhel} < 6
export CC=gcc44
export CXX=g++44
CXXFLAGS="-pthread -I/usr/include/boost141" LDFLAGS="-L/usr/lib64/boost141" %{cmake28} -DBoost_DIR=/usr/lib64/boost141 -DBOOST_INCLUDEDIR=/usr/include/boost141 -DCMAKE_CXX_COMPILER=g++44 -DCMAKE_C_COMPILER=gcc44 -DCOCAINE_LIBDIR=%{_libdir} .
%else
%{cmake28} -DCOCAINE_LIBDIR=%{_libdir} .
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

%post -p /sbin/ldconfig
%postun -p /sbin/ldconfig

%clean
rm -rf %{buildroot}

%files
%defattr(-,root,root,-)
%doc README.md
%{_libdir}/*.so.*

%files devel
%defattr(-,root,root,-)
%{_includedir}/*
%{_libdir}/libcocaine-core.so
%{_libdir}/libjson.so

%files -n cocaine-runtime
%defattr(-,root,root,-)
%{_bindir}/cocaine-runtime
%{_sysconfdir}/init.d/*
%{_sysconfdir}/cocaine/cocaine-default.conf
