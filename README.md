### VExtension for [NTCore Explorer Suite aka CFF Explorer](https://ntcore.com/?page_id=388)
_ _ _

#### Features

>Undecorate the decorated C++ symbol names in the `Import/Export Directory` tab.
>
>Resolve symbol name for symbols that imported by `Ordinal` in the `Import Directory` tab.
>
>Shorten symbol name for several C++ libraries such as STL.

#### Usage

>Step 1. Install the required package [Redistributable 2019](https://aka.ms/vs/17/release/vc_redist.x64.exe) `(ignore if installed)`.
>
>Step 2. Copy the [VExtension](bins/VExtension/) folder inside the `bins` folder to the `Explorer Suite\Extensions\CFF Explorer\` directory.
>
>Step 3. Enjoy.

#### Screenshots

>![](screenshots/undecorate-shorten-name.png?)
>
>![](screenshots/resolve-ordinal.png?)

#### Development

>Step 1. Required Visual Studio C++ 2019 or later.
>
>Step 2. Install [Vutils](https://github.com/vic4key/Vutils.git) library
>
>Step 3. Check [this](https://github.com/vic4key/CFF_VExtension.git) repository and start to work.

#### TODO

>- [ ] Release `32-bit` version for this extension.
>
>- [x] Resolve symbol name for `Ordinal` if they are possible.
>
>- [x] Shorten symbol name for several C++ libraries such as STL, Boost, Etc. Eg. `std::basic_ostream<char,struct std::char_traits<char>>` to `std::ostream`, etc (Doing).
_ _ _

Website: https://vic.onl/
