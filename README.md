# Crash Native Windows — executáveis prontos pelo GitHub Actions

Este repositório já contém a configuração de compilação Windows e o **pacote de fontes sem ROM** em `build-input/Crash-Windows-Source.zip`. Você não precisa instalar Visual Studio ou CMake para baixar os programas produzidos.

## Baixar o Crash Native reconstruído (jogável, mas separado da ROM traduzida)

1. Abra a aba **Actions** do repositório.
2. Escolha **Build Windows - Crash Native e Port GBA**.
3. Abra uma execução com marca verde. Se nenhuma aparecer, use **Run workflow** e aguarde.
4. Em **Artifacts**, baixe `CrashNative-Windows-x64`.
5. Extraia o ZIP baixado apenas uma vez; coloque sua cópia legítima da ROM em `input/crash.gba`, ao lado das pastas e do executável.
6. Abra **CrashNative.exe**. Não precisa compilar nada no PC.

O mesmo workflow entrega `PortGBABuilder-Windows-x64`, que é a ferramenta de tradução, **não o jogo**. Os artefatos agora possuem apenas uma camada ZIP: basta extrair, sem descompactar um ZIP dentro do outro.

## Runtime original ARM/Thumb do Crash (experimental)

**ATENÇÃO:** o artefato `Crash-ROM-Original-Experimental-Windows-x64` só aparece quando o workflow encontra o pacote opcional `build-input/Crash-Experimental-Generated.zip`. Esse pacote precisa conter o **projeto C++ da ROM já gerado**, incluindo `CMakeLists.txt`, na raiz do ZIP. **Não deve conter** arquivo `.gba`, ROM nem save.

**Esse pacote experimental ainda não foi enviado e seu build só é habilitado em repositório privado**: a tradução de instruções da ROM é derivada de conteúdo protegido por direitos autorais e este repositório foi criado como **público**. Antes de enviá-lo, altere a visibilidade em **Settings → General → Danger Zone → Change repository visibility → Private**. Ao mudar para privado, poderemos enviar esse pacote e tentar compilar a janela experimental `ACQE_ROM_Window.exe`.

O runtime original demonstrou abertura, mapa, início de Jungle Jam e resposta aos botões nos testes Linux. **A jogabilidade completa não foi validada**. Um build bem-sucedido não significa que a fase esteja totalmente funcional.

## Se o build falhar

Em **Actions**, abra a execução vermelha e selecione **Compilar CrashNative.exe** ou **Compilar e testar Builder** para ver o erro. Os builds Windows ainda precisam de validação com o MSVC.

## Segurança e dependências

- ROMs e saves não estão no repositório ou no ZIP de fontes.
- O Crash Native reconstruído ainda carrega `input/crash.gba` quando executado; não é completamente independente da ROM.
- O Builder é compilado a partir da v2.13.0 e o Crash reconstruído usa fontes v0.68.0.
- O arquivo `build-input/Crash-Windows-Source.zip` foi verificado e não contém `.gba`, `.srm` ou executáveis pré-fabricados.
