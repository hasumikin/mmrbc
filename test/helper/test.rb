#! /usr/bin/env ruby

class MmrubyTest
  def initialize
    @@pending = false
    @@exit_code = 0
    @@success_count = 0
    @@pending_count = 0
    @@brown = "\e[33m"
    @@green = "\e[32;1m"
    @@red = "\e[31;1m"
    @@reset = "\e[m"
    @@failure_struct = Struct.new(:filename, :description, :script, :expected, :actual)
    @@failures = []
    @@description = ""
    @@mmruby_path = File.expand_path('../../../build/host-production/bin/mmruby', __FILE__)
  end

  def exit_code
    @@exit_code
  end

  def test
    puts 'Start test'
    Dir.glob(File.expand_path('../../*_test.rb', __FILE__)).each do |file|
      @@filename = File.basename(file)
      load file
      @@pending = false
    end
    puts
    puts "Summary:"
    print @@green
    puts "  Success: #{@@success_count}"
    print @@brown if @@pending_count > 0
    puts "  Pending: #{@@pending_count}#{@@reset}"
    if @@failures.count > 0
      print @@red
    else
      print @@green
    end
    puts "  Failure: #{@@failures.count}"
    @@failures.each do |failure|
      puts "    File: #{failure.filename}"
      puts "      Description: #{failure.description}"
      puts "      Script:"
      puts "        #{failure.script.gsub(/\n/, %Q/\n        /)}"
      puts "      Expected:"
      puts "        #{failure.expected.gsub(/\n/, %Q/\n        /)}"
      puts "      Actual:"
      puts "        #{failure.actual.gsub(/\n/, %Q/\n        /)}"
    end
    print @@reset
  end

  def self.desc(text)
    @@description = text
  end

  def self.assert_equal(script, expected)
    if @@pending
      print "#{@@brown}.#{@@reset}"
      @@pending_count += 1
      return
    end
    actual = `#{@@mmruby_path} -e "#{script}"`.chomp.gsub(/\r/, "")
    if actual == expected
      print "#{@@green}."
      @@success_count += 1
    else
      print "#{@@red}."
      @@failures << @@failure_struct.new(@@filename, @@description, script.chomp, expected, actual)
      @@exit_code = 1
    end
    print @@reset
  end

  def self.pending
    @@pending = true
  end
end

mmruby_test = MmrubyTest.new
mmruby_test.test

exit mmruby_test.exit_code
