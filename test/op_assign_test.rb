class OpAssignTest < MmrubyTest
  desc "OP_ASSIGN with both []= and attr="
  assert_equal(<<~RUBY, "[4, 6]")
    class Array
      def first
        self[0]
      end
      def first=(val)
        self[0] = val
      end
    end
    ary = [1, 2]
    ary.first += 3
    ary[1] *= 3
    p ary
  RUBY
end

