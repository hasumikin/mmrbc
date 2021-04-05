class IfTest < PicoRubyTest
  desc "use if value 1"
  assert_equal(<<~RUBY, '1')
    res = if true
      1
    else
      0
    end
    p res
  RUBY

  desc "use if value 2"
  assert_equal(<<~RUBY, '0')
    res = if false
      1
    else
      0
    end
    p res
  RUBY

  desc "use if value 3"
  assert_equal(<<~RUBY, "0")
    ary = [0]
    res = if true
      ary[0]
    end
    puts res
  RUBY

  desc "use else value 1"
  assert_equal(<<~RUBY, "false")
    ary = [0]
    res = if !true
      ary[0]
    else
      ary[1]
      false
    end
    puts res
  RUBY

  desc "a complicated case"
  assert_equal(<<~RUBY, "hello\ntrue\n1\nnil")
    proc = Proc.new do puts 'hello' end
    ary = [0, true, proc]
    res = if ary[1]
      ary[0] += 1
      ary[2].call
      ary[1]
    end
    puts res
    res = if false
    else
      puts ary[0]
    end
    p res
  RUBY

  desc "conditilnal operator 1"
  assert_equal(<<~RUBY, "true")
    a = [true, false]
    res = !nil ? a[0] : a[1]
    puts res
  RUBY

  desc "conditilnal operator 2"
  assert_equal(<<~RUBY, "false")
    a = [true, false]
    res = !true ? a[0] : a[1]
    puts res
  RUBY
end

