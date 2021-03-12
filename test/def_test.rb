class DefTest < MmrubyTest
  desc "def method without arg"
  assert_equal(<<~RUBY, "My method!")
    def my_method
      'My method!'
    end
    puts my_method
  RUBY

  desc "def method with a mandatory arg"
  assert_equal(<<~RUBY, "My arg")
    def my_method(arg)
      arg
    end
    puts my_method('My arg')
  RUBY

  desc "endless def"
  assert_equal(<<~RUBY, "0")
    def my_method(arg) = arg.to_i
    puts my_method(nil)
  RUBY

  pending

  desc "def method with an optional arg"
  assert_equal(<<~RUBY, "default")
    def my_method(arg = 'defalut')
      arg
    end
    puts my_method
  RUBY
end
