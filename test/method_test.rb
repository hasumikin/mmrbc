class MethodTest < MmrubyTest
  desc "Integer class"
  assert_equal(<<~RUBY, "1234")
    puts 1234.to_s
  RUBY

  desc "Method chain"
  assert_equal(<<~RUBY, "1234")
    puts 1234.to_s.to_i
  RUBY
end
