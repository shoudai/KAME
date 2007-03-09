%global kamedate kame_date
%global qtver 3.3
%global kdever 3.3
%global ftglver 2.1.2
%global mikachanver 8.9

Name: kame

%if 0%{?without_nidaqmx}
%global kamerel wonidaqmx
%global wnidaqmx_param no
%else
%global wnidaqmx_param yes
%global without_nidaqmx 0
%endif

Version: 2.1.3
Release: 1%{?kamerel}
License: GPL
Group: Applications/Engineering
BuildRoot:      %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
Requires: qt >= %{qtver}, kdelibs >= %{kdever}
Requires: libart_lgpl, gsl, fftw2, zlib, ruby
Requires: linux-gpib
BuildPreReq: ruby-devel, gsl, boost-devel, fftw2-devel
BuildPreReq: libidn-devel
BuildPreReq: qt-devel >= %{qtver}, kdelibs-devel >= %{kdever}
BuildPreReq: libart_lgpl-devel, zlib-devel, libpng-devel, libjpeg-devel
BuildPreReq: gcc-c++ >= 3.3
BuildPreReq: compat-gcc-34-c++
BuildPreReq: linux-gpib-devel

%if !%{without_nidaqmx}
BuildPreReq: nidaqmxcapii
Requires: nidaqmxef
%endif

Source0: %{name}-%{version}-%{kamedate}.tar.bz2
Source1: ftgl-%{ftglver}.tar.gz
Source2: mikachanfont-%{mikachanver}.tar.bz2

Summary: KAME, K's adaptive measurement engine.

%description
K's adaptive measurement engine. 

%prep
%setup -q -a 1 -a 2 -n %{name}-%{version}-%{kamedate}
mv mikachanfont-%{mikachanver}/fonts/* kame/mikachanfont
mv mikachanfont-%{mikachanver}/* kame/mikachanfont


%build
# build static FTGL
pushd FTGL/unix
CXX=g++34 ./configure --disable-shared --enable-static
make ##%%{?_smp_mflags}
popd

%configure --enable-debug --with-nidaqmx=%{wnidaqmx_param}
make ##%%{?_smp_mflags}

%install
rm -rf $RPM_BUILD_ROOT
%makeinstall
#if [ -f $RPM_BUILD_ROOT/%{_bindir}/*-kame ]
#then
#	mv $RPM_BUILD_ROOT/%{_bindir}/*-kame $RPM_BUILD_ROOT/%{_bindir}/kame
#fi

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
%{_bindir}/kame
%{_datadir}/applnk/Applications/*.desktop
%{_datadir}/apps/kame
%{_datadir}/icons/*/*/apps/*.png
%{_datadir}/locale/*/LC_MESSAGES/*.mo
%{_datadir}/doc/HTML/*/kame

%changelog
