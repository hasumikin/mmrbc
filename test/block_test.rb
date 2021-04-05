class BlockTest < PicoRubyTest
  desc "do block"
  assert_equal(<<~RUBY, "0\n1")
    [0, 1].each do |i|
      puts i
    end
  RUBY

  desc "do block chain"
  assert_equal(<<~RUBY, "1\n2")
    a = [0, 1]
    a.each do |i|
      a[i] += 1
    end.each do |i|
      puts i
    end
  RUBY

  desc "brace block"
  assert_equal(<<~RUBY, "0\n1")
    [0, 1].each { |i| puts i }
  RUBY

  desc "brace block chain"
  assert_equal(<<~RUBY, "1\n2")
    a = [0, 1]
    a.each { |i| a[i] += 1 }.each { |i| puts i }
  RUBY

  desc "Hash#each do |k, v|"
  assert_equal(<<~RUBY, "a\n0\nb\n1")
    h = {a: 0, b: 1}
    h.each do |k, v|
      puts k
      puts v
    end
  RUBY

  desc "each_with_index"
  assert_equal(<<~RUBY, "true\n0\nfalse\n1")
    a = [true, false]
    a.each_with_index do |val, index|
      puts val
      puts index
    end
  RUBY

end
