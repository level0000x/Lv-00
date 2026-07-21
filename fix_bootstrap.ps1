$file = (Get-Item "core\src\layer2_resource\bootstrap_test.c").FullName
$content = Get-Content $file -Raw -Encoding UTF8
$content = $content -replace 'bool bootstrap_test_framework_init\(', 'int bootstrap_test_framework_init('
$content = $content -replace 'bool graph_isomorphism_compare\(', 'int graph_isomorphism_compare('
$content = $content -replace 'bool graph_isomorphism_find_mapping\(', 'int graph_isomorphism_find_mapping('
$content = $content -replace 'bool primitive_wrapper_init\(', 'int primitive_wrapper_init('
$content = $content -replace 'bool primitive_wrapper_register\(', 'int primitive_wrapper_register('
$content = $content -replace 'bool test_oracle_verify_normalization_idempotent\(', 'int test_oracle_verify_normalization_idempotent('
$content = $content -replace 'bool test_oracle_verify_solution_correct\(', 'int test_oracle_verify_solution_correct('
$content = $content -replace 'bool test_oracle_verify_proof_valid\(', 'int test_oracle_verify_proof_valid('
$content = $content -replace 'bool test_oracle_verify_serialize_roundtrip\(', 'int test_oracle_verify_serialize_roundtrip('
$content = $content -replace 'bool bootstrap_test_write_report\(', 'int bootstrap_test_write_report('
$content = $content -replace 'return false;', 'return -1;'
$content = $content -replace 'return true;', 'return 0;'
Set-Content $file $content -NoNewline -Encoding UTF8
Write-Host "Done"