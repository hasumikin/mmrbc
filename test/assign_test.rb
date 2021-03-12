class AssignTest < MmrubyTest
  assert_equal(<<~RUBY, "1")
    a = 1
    puts a
  RUBY
end
