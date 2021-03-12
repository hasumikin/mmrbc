[![C/C++ CI](https://github.com/hasumikin/mmruby/actions/workflows/c-cpp.yml/badge.svg)](https://github.com/hasumikin/mmruby/actions/workflows/c-cpp.yml)

## mmruby

mmruby is an alternative mruby implementation which is:

- Small foot print
  - ROM: 256KB or less
  - RAM: 128KB or less
  - Reference board is PSoC5LP which has Arm 32bit processor, 256KB ROM and 64KB RAM
- Portable
  - Depends on only standard C library such as glibc, Newlib or Newlib-nano

### Set-up

- mmrbc
  - mini mruby compiler. The main part of this repository
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

- mmrbc
  - `build/host-debug/bin/mmrbc source.rb` makes `source.mrb` which is VM code runs on mruby VM
- mmruby
  - `build/host-debug/bin/mmruby source.rb` executes Ruby just like normal `ruby` command
  - You can also do like `build/host-debug/bin/mmruby -e 'puts "Hello World!"'`
- mmirb
  - It is an experimental REPL implementation. See [mmirb section](#mmirb)

### Debug build and production build

`make` command makes "debug build" which shows debug-print like this:

![](https://raw.githubusercontent.com/hasumikin/mmruby/master/docs/images/debug-print.png)

You can get "production build" like `build/host-production/bin/mmruby` which omits debug-print by `make host_production`

### mmirb<a name="mmirb"></a>

Because mmruby is dedicated for onechip-microcontroller which doesn't have STDIN/STDOUT like *normal computers*, REPL (or you can also say SHELL) should work over serial communication like UART.

You can start up mmirb on your host machine with `make mmirb` though, it may look weird since it is implemented as a ported version from microcontroller version.
Please try to use it according as the message on the screen if you are intersted in.

You will find more information at [hasumikin/mruby_machine_PSoC5LP](https://github.com/hasumikin/mruby_machine_PSoC5LP)

### Presentation about mmruby

I gave a talk about this project on [RubyKaigi Takeout 2020](https://rubykaigi.org/2020-takeout).

- [Video](https://youtu.be/kDOf_tZKlLU)
- [Slide](https://slide.rabbit-shocker.org/authors/hasumikin/RubyKaigiTakeout2020/)

### Roadmap

mmruby is still developing halfway towards finishing as of 2020.

See implementation roadmap on [issue/6](https://github.com/hasumikin/mmruby/issues/6)

### Contributing to mmruby

Fork, fix, then send a pull request.

### Acknowledgement

Part of this project was coded by [Monstarlab](https://monstar-lab.com/) with the support of [the Ruby Association Grant Program 2020](https://www.ruby.or.jp/en/news/20201022).

### License

Copyright © 2020-2021 HASUMI Hitoshi & Monstarlab. See MIT-LICENSE for further details.
