# Crash Windows - Como baixar e testar (atualizado v2.14.0)

**Sem instalar compiladores no PC:** o GitHub Actions compila os executaveis e entrega o ZIP portatil.

## Teste disponivel neste repositorio PUBLICO: Crash reconstruido
1. Clique na aba **Actions**: https://github.com/jogoiarexx-blip/Crash-Native-Windows./actions
2. Escolha **Build Windows - Crash Native e Port GBA** e abra um build **verde**; se nao houver build, use **Run workflow**.
3. Em **Artifacts**, baixe `CrashNative-Windows-x64`.
4. Extraia o ZIP **uma vez**. Nao execute `.cmd` de compilacao.
5. Coloque sua copia legitima da ROM em `input/crash.gba`.
6. Execute `CrashNative.exe`. O Builder `PortGBABuilder-Windows-x64` e separado.

## Teste do Crash Hybrid (ROM original + reconstrucao no mesmo EXE)
O alvo `CrashHybrid.exe` foi adicionado, mas **nao esta compilado nem validado no Windows**. Para gerar o artifact `CrashHybrid-Windows-x64`, ainda precisa:

- Alterar este repositorio para **Private** em Settings > General > Danger Zone.
- Enviar a fonte C++ da traducao (sem ROM) para `build-input/Crash-Experimental-Generated.zip` **apenas depois de privado**.
- Rodar novamente o workflow. Ele tentara produzir `CrashHybrid-Windows-x64`; so baixe se o job terminar verde.
- No PC: extrair, colocar a ROM em `input/crash.gba`, abrir `CrashHybrid.exe`. **SIM** = traducao ARM/Thumb experimental; **NAO** = motor reconstruido. `--check` verifica a instalacao.

O hibrido ainda **nao une fisica, saves ou gameplay** e **nao concluiu a fase** pelo runtime original. A execucao Windows ainda precisa de teste.

## Port GBA Builder v2.14.0
O workflow aplica `build-input/PortGBA-v2.14.0-CI-overlay.b64` aos fontes de v2.13.0. Foram adicionados:
- Limitacao de frames da janela gerada ao ritmo do GBA (~59,73 FPS).
- `package-windows`, que valida o arquivo PE Windows x64 e monta pasta portatil sem incluir ROM.
- Testes Linux: **13/13** Builder. Projeto C++ da ROM regenerado e executavel de console compilado no Linux; janela Win32 ainda nao testada.

---

# Crash Native Windows — executáveis prontos pelo GitHub Actions

Este repositório já contém a configuração de compilação Windows e o **pacote de fontes sem ROM** em `build-input/Crash-Windows-Source.zip`. Você não precisa instalar Visual Studio ou CMake para baixar os programas produzidos.

## Crash Hibrido: um so EXE com dois motores (fase inicial)

O codigo-fonte do **CrashHybrid.exe** esta em `hybrid/`. Ele incorpora o runtime ARM/Thumb original e o Crash reconstruido no **mesmo executavel**, com escolha no inicio: **SIM = original experimental; NAO = reconstruido**. Isso ainda **nao unifica** saves, fisica ou estado de gameplay; nao ha mudanca automatica de motores durante uma fase.

O workflow tentara gerar o artifact `CrashHybrid-Windows-x64` **somente se** o repositorio for **privado** e contiver `build-input/Crash-Experimental-Generated.zip`, com o codigo C++ gerado e sem o arquivo da ROM. Nao publique esse codigo derivado da ROM em repositorios publicos.

**Estado atual:** fonte do integrador publicada, teste de selecao feito localmente; compilacao do executavel hibrido no Windows ainda pendente. Mesmo compilado, ambos os modos precisam da sua copia legitima em `input/crash.gba`.

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
