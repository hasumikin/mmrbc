[![C/C++ CI](https://github.com/hasumikin/picoruby/actions/workflows/c-cpp.yml/badge.svg)](https://github.com/hasumikin/picoruby/actions/workflows/c-cpp.yml)

## PicoRuby ~~mmruby~~ (renamed on March 15th, 2021)

PicoRuby is an alternative mruby implementation which is:

- Small foot print
  - ROM: 256KB or less
  - RAM: 128KB or less
  - Reference board is PSoC5LP which has Arm 32bit processor, 256KB ROM and 64KB RAM
- Portable
  - Depends on only standard C library such as glibc, Newlib or Newlib-nano

### Set-up

- picorbc
  - pico mruby compiler. The main part of this repository
- [mruby/c (mrubyc/mrubyc)](https://github.com/mrubyc/mrubyc)
  - Another implementation of mruby virtual machine

### Build

- Just hitting `make` will build binaries for your host machine.
- You can build library files for PSoC5LP by hitting `make psoc5lp_lib`
  - It requires you to have Docker though,
  - You can make arm-none-eabi tools if you don't want to use Docker
  - [hasumikin/cross_compilation_toolchains](https://github.com/hasumikin/cross_compilation_toolchains) may help you to make an environment

### Binaries

`make` command will make three executable binaries

- picorbc
  - `build/host-debug/bin/picorbc source.rb` makes `source.mrb` which is VM code runs on mruby VM
- picoruby
  - `build/host-debug/bin/picoruby source.rb` executes Ruby just like normal `ruby` command
  - You can also do it like `build/host-debug/bin/picoruby -e 'puts "Hello World!"'`
- picoshell
  - It is an experimental REPL implementation. See [picoshell section](#picoshell)

### Debug build and production build

`make` command makes "debug build" which shows debug-print like this:

![](https://raw.githubusercontent.com/hasumikin/picoruby/master/docs/images/debug-print.png)

(Please replace `mmruby` with `picoruby`)

You can get "production build" like `build/host-production/bin/picoruby` which omits debug-print by `make host_production`

### picoshell<a name="picoshell"></a>

Because PicoRuby is dedicated for onechip-microcontroller which doesn't have STDIN/STDOUT like *normal computers*, REPL (or you can also say SHELL) should work over serial communication like UART.

You can start up picoshell on your host machine with `make picoshell` though, it may look weird since it is implemented as a ported version from microcontroller version.
Please try to use it according as the message on the screen if you are intersted in.

You will find more information at [hasumikin/mruby_machine_PSoC5LP](https://github.com/hasumikin/mruby_machine_PSoC5LP)

### Presentation about ~~mmruby~~ PicoRuby

I gave a talk about this project on [RubyKaigi Takeout 2020](https://rubykaigi.org/2020-takeout).

- [Video](https://youtu.be/kDOf_tZKlLU)
- [Slide](https://slide.rabbit-shocker.org/authors/hasumikin/RubyKaigiTakeout2020/)

### Roadmap

PicoRuby is still developing halfway towards finishing as of 2020.

See implementation roadmap on [issue/6](https://github.com/hasumikin/picoruby/issues/6)

### Contributing to PicoRuby

Fork, fix, then send a pull request.

### Acknowledgement

Part of this project was coded by [Monstarlab](https://monstar-lab.com/) with the support of [the Ruby Association Grant Program 2020](https://www.ruby.or.jp/en/news/20201022).

### License

Copyright © 2020-2021 HASUMI Hitoshi & Monstarlab. See MIT-LICENSE for further details.
