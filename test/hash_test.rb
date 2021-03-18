class HashTest < PicoRubyTest
  desc "Generating a hash"
  assert_equal(<<~RUBY, '{:a=>1, "b"=>"2", :c=>true}')
    hash = {a: 1, 'b' => '2', c: true}
    p hash
  RUBY
end
