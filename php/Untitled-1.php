<!-- <!-- <?php
// mysql 객체 생성
$DB = mysqli_connect('IP', '계정', '패스워드', '데이터베이스', '포트');
// @을 붙이면 php가 페이지에 로그 찍는것을 막음
$DB = @mysqli_connect('IP', '계정', '패스워드', '데이터베이스', '포트');

// 실패하면 널나옴
if(!$DB)
{

}

// 언어셋 맞춰 줌
// mysqli_set_charset($DB, "utf8");

// DB에 쿼리문 날리기
mysqli_query($DB, $QUERY);

// 인코딩 방법 변경
// iconv(~~~);

// 패치를 할 때 메모리에 올려놓고 낫개로 하나씩 줌
// 널이 나올 때 까지 반복적으로 함수를 사용하여 뽑아주면 됨
// 널 체크 해 줘야됨
// NUM으로 뽑아주면 컬럼의 순서대로 뽑아줘야됨
// ASSOC은 상관없음
// mysqli_fetch_array(~~~);

// 다 썼으면 해제
// mysqli_free_result(~);

// DB 연결 끊기
// 안해도 무관함
// php 로직중에 DB를 사용 안한다면 그 때 끊어주는 것이 좋음
mysqli_close($DB);


?>