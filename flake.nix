{
  description = "UPPAAL CDD Library";

  inputs.nixpkgs.url = "nixpkgs/master";
  inputs.uutils.url = "github:UPPAALModelChecker/UUtils";
  inputs.udbm.url = "github:UPPAALModelChecker/UDBM";

  outputs = { self, nixpkgs, uutils, udbm }:
  let
      # System types to support.
      supportedSystems = [ "x86_64-linux" "x86_64-darwin" "aarch64-linux" "aarch64-darwin" ];
      # Helper function to generate an attrset '{ x86_64-linux = f "x86_64-linux"; ... }'.
      forAllSystems = nixpkgs.lib.genAttrs supportedSystems;
      # Nixpkgs instantiated for supported system types.
      nixpkgsFor = forAllSystems (system: import nixpkgs { inherit system; });
      crossNixpkgsFor = forAllSystems (system: import nixpkgs {
          inherit system;
          crossSystem = nixpkgs.lib.systems.examples.mingwW64;
      });

  in
  rec {
      library = (pkgs: nativePkgs:
      uutils.addCrossCheck pkgs nativePkgs (pkgs.stdenv.mkDerivation {
          pname = "UCDD";
          version = "0.2.1";
          src = ./.;
          nativeBuildInputs = with nativePkgs; [ cmake ];
          propagatedBuildInputs = [ (uutils.library pkgs nativePkgs) (udbm.library pkgs nativePkgs) ];
          buildInputs = with pkgs; [ doctest boost174 ];
          cmakeFlags = [ "-DTESTING=ON" ];
          doCheck = true;
      }));


    packages.ucdd = forAllSystems (system:
    let
      pkgs = nixpkgsFor.${system};
    in
    self.library pkgs pkgs);

    packages.crossPackage = forAllSystems (system:
        let
          nativePkgs = nixpkgsFor.${system};
          pkgs = crossNixpkgsFor.${system};
        in
        (self.library pkgs nativePkgs));

    defaultPackage = packages.ucdd;
  };
}
