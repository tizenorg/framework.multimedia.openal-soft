Name:       openal-soft
Summary:    OpenAL library software implementation
Version:    1.13
Release:    4
Group:      Multimedia/openal-soft
License:    LGPLv2+ 
Source0:    %{name}-%{version}.tar.gz
BuildRequires: cmake
BuildRequires: pkgconfig(avsysaudio)
BuildRequires: pkgconfig(mm-session)
BuildRequires: pkgconfig(audio-session-mgr)
BuildRequires: pkgconfig(vconf)
Requires(post): /sbin/ldconfig
Requires(postun): /sbin/ldconfig


%description
OpenAL library software implementation



%package devel
Summary:    OpenAL library software implementation (devel)
Group:      Development/Libraries
Requires:   %{name} = %{version}-%{release}

%description devel
OpenAL library software implementation development package


%prep
%setup -q

%build

%ifarch %arm
export CFLAGS+=" -DARM_ARCH -O3 -ftree-vectorize -ffast-math -fsingle-precision-constant -DUSE_DLOG "
%else
export CFLAGS+=" -DI386_ARCH "
%endif

cmake . -DCMAKE_INSTALL_PREFIX=%{_prefix}
make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}
%make_install


%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig



%files
%{_libdir}/*.so*
%{_bindir}/*
/etc/openal/alsoft.conf

%files devel
%{_includedir}/*
%{_libdir}/pkgconfig/*

