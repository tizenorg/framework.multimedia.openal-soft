Name:       openal-soft
Summary:    OpenAL library software implementation
Version:    1.14
Release:    1
Group:      Multimedia/openal-soft
License:    LGPLv2+
Source0:    %{name}-%{version}.tar.gz
BuildRequires: cmake
BuildRequires: pkgconfig(mm-session)
BuildRequires: pkgconfig(libpulse)
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
export CFLAGS+=" -DARM_ARCH -O3 -ftree-vectorize -ffast-math -fsingle-precision-constant -DUSE_ASM_IN_PULSEAUDIO -DUSE_DLOG "
%else
export CFLAGS+=" -DI386_ARCH -DUSE_ASM_IN_PULSEAUDIO "
%endif

cmake . -DCMAKE_INSTALL_PREFIX=%{_prefix}
make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}
mkdir -p %{buildroot}/usr/share/license
cp COPYING %{buildroot}/usr/share/license/%{name}
%make_install


%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig



%files
%manifest openal-soft.manifest
%{_libdir}/*.so*
%{_bindir}/*
/etc/openal/alsoft.conf
/usr/share/license/%{name}

%files devel
%{_includedir}/*
%{_libdir}/pkgconfig/*

