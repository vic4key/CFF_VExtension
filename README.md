### VExtension for [NTCore Explorer Suite aka CFF Explorer](https://ntcore.com/?page_id=388)

_ _ _

#### Features

> * Undecorate the decorated C++ symbol names in the `Import/Export Directory` tab.
> 
> * Resolve symbol names for symbols that are imported by `Ordinal` in the `Import Directory` tab.
> 
> * Shorten symbol name for several C++ libraries such as STL (you can easily modify or add more rules in the configuration file [VExtension.json](bins/VExtension/VExtension.json)).
> 
> * Auto Expand the Width of the Name Column in API Tables

#### Usage

> Copy the [VExtension](bins/VExtension/) folder inside the `bins` folder to the `Explorer Suite\Extensions\CFF Explorer` directory.

#### Screenshots

> ![](screenshots/undecorate-shorten-name.png?)
> 
> ![](screenshots/resolve-ordinal.png?)
> 
> ![](screenshots/options.png?)

#### Development

> Step 1. Required Visual Studio C++ 2019 or later.
> 
> Step 2. Install [Vutils](https://github.com/vic4key/Vutils.git) library
> 
> Step 3. Check [this](https://github.com/vic4key/CFF_VExtension.git) repository and start to work.

#### TODO

> - [ ] Release the `32-bit` version.
> 
> - [x] Add GUI for options.
> 
> - [x] Resolve symbol names for symbols that are imported by `Ordinal` if they are possible.
> 
> - [x] Shorten symbol name for several C++ libraries such as STL, Boost, etc. Eg. `std::basic_ostream<char,struct std::char_traits<char>>` to `std::ostream`.
> 
> - [x] Auto fit width the Name column in API Table.
_ _ _

Website: https://vic.onl/
