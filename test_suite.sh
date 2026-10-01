#!/bin/bash
set -e

DB_FILE="test_suite.db"
BIN="./bin/dbview"

# Biên dịch lại với cờ debug
make clean
make default
rm -f "$DB_FILE"

echo "=== BẮT ĐẦU KIỂM THỬ ==="

# 1. Tạo mới database rỗng
$BIN -f "$DB_FILE" -n
# Kích thước phải đúng 12 bytes
[ $(stat -c%s "$DB_FILE") -eq 12 ] && echo "Pass: Init DB header 12 bytes"

# 2. Thêm nhân viên đầu tiên
$BIN -f "$DB_FILE" -a "Nguyen Van A,Hanoi,160"
# Kích thước phải đúng 12 + 516 = 528 bytes
[ $(stat -c%s "$DB_FILE") -eq 528 ] && echo "Pass: Add 1st employee (528 bytes)"

# 3. Thêm nhân viên thứ hai
$BIN -f "$DB_FILE" -a "Tran Thi B,Da Nang,180"
# Kích thước phải đúng 12 + 516*2 = 1044 bytes
[ $(stat -c%s "$DB_FILE") -eq 1044 ] && echo "Pass: Add 2nd employee (1044 bytes)"

# 4. Thêm nhân viên thứ ba
$BIN -f "$DB_FILE" -a "Le Van C,Saigon,200"
# Kích thước phải đúng 12 + 516*3 = 1560 bytes
[ $(stat -c%s "$DB_FILE") -eq 1560 ] && echo "Pass: Add 3rd employee (1560 bytes)"

# 1. Liệt kê toàn bộ
$BIN -f "$DB_FILE" -l

# 2. Tìm kiếm nhân viên có tồn tại
$BIN -f "$DB_FILE" -s "Tran Thi B"

# 3. Tìm kiếm nhân viên không tồn tại (không được crash)
$BIN -f "$DB_FILE" -s "Unknown User"

# Cập nhật địa chỉ và giờ làm cho Tran Thi B
$BIN -f "$DB_FILE" -u "Tran Thi B,Hue,210"

# Tìm kiếm lại để kiểm chứng thông tin mới
$BIN -f "$DB_FILE" -s "Tran Thi B"

# 1. Xóa nhân viên ở giữa (Tran Thi B)
$BIN -f "$DB_FILE" -d "Tran Thi B"
# Kích thước file BẮT BUỘC phải giảm về 1044 bytes (nhờ ftruncate)
[ $(stat -c%s "$DB_FILE") -eq 1044 ] && echo "Pass: Shrink file after delete (1044 bytes)"

# 2. Xác thực lại tính toàn vẹn của file (Header filesize phải khớp st_size)
$BIN -f "$DB_FILE" -l

rm -f empty.db
$BIN -f empty.db -n

# Liệt kê trên DB rỗng
$BIN -f empty.db -l

# Tìm kiếm trên DB rỗng
$BIN -f empty.db -s "Nguyen Van A"

# Xóa trên DB rỗng
$BIN -f empty.db -d "Nguyen Van A" || true

# Update trên DB rỗng
$BIN -f empty.db -u "Nguyen Van A,Hanoi,100" || true

rm -f empty.db

rm -f shrink_test.db
$BIN -f shrink_test.db -n
$BIN -f shrink_test.db -a "Emp1,Addr1,100"
$BIN -f shrink_test.db -a "Emp2,Addr2,200"

# Xóa người thứ nhất
$BIN -f shrink_test.db -d "Emp1"
[ $(stat -c%s shrink_test.db) -eq 528 ]

# Xóa người cuối cùng (DB trở về 0 nhân viên)
$BIN -f shrink_test.db -d "Emp2"
# Kích thước phải rút gọn về đúng 12 bytes ban đầu
[ $(stat -c%s shrink_test.db) -eq 12 ] && echo "Pass: DB shrank back to pure header (12 bytes)"

# Thêm lại nhân viên mới vào DB vừa bị xóa sạch để kiểm tra con trỏ tái sử dụng
$BIN -f shrink_test.db -a "New Emp,Addr,150"
[ $(stat -c%s shrink_test.db) -eq 528 ] && echo "Pass: Successfully re-added after full wipe"

rm -f shrink_test.db

rm -f boundary.db
$BIN -f boundary.db -n
$BIN -f boundary.db -a "First,Addr,10"
$BIN -f boundary.db -a "Middle,Addr,20"
$BIN -f boundary.db -a "Last,Addr,30"

# Xóa phần tử đầu (index 0 - kiểm tra dịch chuyển mảng)
$BIN -f boundary.db -d "First"
$BIN -f boundary.db -l

# Xóa phần tử cuối (vòng lặp memmove/for không bị tràn biên)
$BIN -f boundary.db -d "Last"
$BIN -f boundary.db -l

rm -f boundary.db

rm -f malformed.db
$BIN -f malformed.db -n

# Thiếu trường hours
$BIN -f malformed.db -a "OnlyName,OnlyAddr" || true

# Chuỗi rỗng / chỉ có dấu phẩy
$BIN -f malformed.db -a ",," || true

# Update chuỗi sai định dạng
$BIN -f malformed.db -u "MalformedUpdate" || true

rm -f malformed.db

rm -f corrupt.db
$BIN -f corrupt.db -n
$BIN -f corrupt.db -a "Test,Addr,10"

# Cố tình ghi thêm byte rác vào đuôi file để làm lệch kích thước với dbstat.st_size
echo "corrupt_data" >> corrupt.db

# Chạy lệnh kiểm tra xem validate_db_header có phát hiện ra không
$BIN -f corrupt.db -l || echo "Pass: Caught corrupted database successfully"

rm -f corrupt.db

rm -f valgrind.db

# 1. Init + Add
valgrind --leak-check=full --error-exitcode=1 $BIN -f valgrind.db -n -a "Valgrind User,HN,100"

# 2. Read / List / Search
valgrind --leak-check=full --error-exitcode=1 $BIN -f valgrind.db -l
valgrind --leak-check=full --error-exitcode=1 $BIN -f valgrind.db -s "Valgrind User"
valgrind --leak-check=full --error-exitcode=1 $BIN -f valgrind.db -s "NonExistent"

# 3. Update
valgrind --leak-check=full --error-exitcode=1 $BIN -f valgrind.db -u "Valgrind User,SG,200"

# 4. Delete
valgrind --leak-check=full --error-exitcode=1 $BIN -f valgrind.db -d "Valgrind User"

rm -f valgrind.db
