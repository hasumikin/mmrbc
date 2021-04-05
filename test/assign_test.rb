class AssignTest < PicoRubyTest
  assert_equal(<<~RUBY, "1")
    a = 1
    puts a
  RUBY

  assert_equal(<<~RUBY, '"hello"')
    a = b = 'hello'
    p b
  RUBY

  pending

  assert_equal(<<~RUBY, '"hello"')
    a = b = 'hello'
    p a
  RUBY
end
